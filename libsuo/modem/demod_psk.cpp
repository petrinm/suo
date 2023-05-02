#include <string>
#include <assert.h>
#include <iostream>

#include "modem/demod_psk.hpp"
#include "registry.hpp"


using namespace std;
using namespace suo;


/*


https://github.com/jgaeddert/liquid-dsp/blob/master/examples/nco_pll_modem_example.c
https://github.com/jgaeddert/liquid-dsp/blob/master/examples/symsync_crcf_example.c#L100

*/

PSKDemodulator::Config::Config() {
	center_frequency = 100;
	samples_per_symbol = 4;
	filter_delay = 5;
	bt = 0.3;
}


PSKDemodulator::PSKDemodulator(const Config& conf) :
	conf(conf)
{

	/* Configure a resampler for a fixed oversampling ratio */
	float resamprate = conf.symbol_rate * conf.samples_per_symbol / conf.sample_rate;
	double bw = 0.4 * resamprate / conf.samples_per_symbol;
	int semilen = lroundf(1.0f / bw);
	l_resamp = resamp_crcf_create(resamprate, semilen, bw, 60.0f, 16);
	resamp_crcf_print(l_resamp);

	/* Calculate maximum number of output samples after feeding one sample
	 * to the resampler. This is needed to allocate a big enough array. */
	resampint = ceilf(1 / resamprate);
	sample_ns = roundf(1.0e9 / conf.sample_rate);

	/* NCO:
	 * Limit AFC range to half of symbol rate to keep it
	 * from wandering too far */
	nco_1Hz = pi2f / conf.sample_rate;
	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_pll_set_bandwidth(l_nco, 0.5 * nco_1Hz);
	update_nco();


	/*
	 * AGC: Automatic gain control
	 */
	l_agc = agc_crcf_create();
	agc_crcf_set_bandwidth(l_agc, 1e-3f);
	//agc_crcf_set_gain_limits(l_agc, 1e-2f, 1e2f);

	l_demod_delay = windowcf_create(4 * conf.samples_per_symbol);


	l_mod = modem_create(LIQUID_MODEM_PSK2);

	/* Symbol synchroniser */
	unsigned int m = 5;                 // filter delay (symbols)
	float        beta = 0.5f;           // filter excess bandwidth factor
	unsigned int num_filters = 32;      // number of filters in the bank
	unsigned int num_symbols = 400;     // number of data symbols

	l_sync = symsync_crcf_create_rnyquist(LIQUID_FIRFILT_ARKAISER,
		conf.samples_per_symbol, m, beta, num_filters);

	symsync_crcf_set_lf_bw(l_sync, conf.symsync_bw0); // loop filter bandwidth

	reset();
}


PSKDemodulator::~PSKDemodulator()
{
	agc_crcf_destroy(l_agc);
	symsync_crcf_destroy(l_sync);
	windowcf_destroy(l_demod_delay);
	modemcf_destroy(l_mod);
}


void PSKDemodulator::reset() {
	agc_crcf_reset(l_agc);
	nco_crcf_reset(l_nco);
}


void PSKDemodulator::update_nco()
{
	conf_dirty = false;

	// Calclate new frequency limits
	float center_frequency = (conf.center_frequency + conf.frequency_offset);
	freq_min = nco_1Hz * (center_frequency - 0.5f * conf.symbol_rate);
	freq_max = nco_1Hz * (center_frequency + 0.5f * conf.symbol_rate);

#if 1
	// Clamp the NCO frequency between new limits
	float old_freq = nco_crcf_get_frequency(l_nco);
	nco_crcf_set_frequency(l_nco, clamp(old_freq, freq_min, freq_max));
#else
	// Set new NCO frequency 
	nco_crcf_set_frequency(l_nco, nco_1Hz * center_frequency);
#endif
}


void PSKDemodulator::sinkSamples(const SampleVector& samples, Timestamp timestamp)
{
	float phase_error;
	
	/* Allocate small buffers from stack */
	Sample samples2[resampint];

	if (conf_dirty && receiver_lock == false)
		update_nco();

	size_t si;
	for (si = 0; si < samples.size(); si++) {
		unsigned nsamp2 = 0, si2;
		Sample s = samples[si];

		/* Downconvert and resample one input sample at a time */
		nco_crcf_step(l_nco);
		nco_crcf_mix_down(l_nco, s, &s);

		Sample r, x_hat;

		enum CostasLoopOrder pll_order = CostasLoopBPSK;
		const float k = sqrt(2.f) - 1.f;

		switch(pll_order) {
		case CostasLoopBPSK:
			phase_error = r.real() * r.imag();
			break;

		case CostasLoopQPSK:
		case CostasLoopGMSK:
			phase_error = copysign(r.imag(), r.real()) - copysign(r.real(), r.imag());
			break;

		case CostasLoop8PSK:
			if (abs(r.real()) >= abs(r.imag()))
				phase_error = copysign(r.imag(), r.real()) - k * copysign(r.real(), r.imag());
			else
				phase_error = k * copysign(r.imag(), r.real()) - copysign(r.real(), r.imag());
			break;
			
		default:
			//phase_error = arg(r * conj(x_hat)); // FM demod
			phase_error = (r * conj(x_hat)).imag(); // Dunno: modemcf_get_demodulator_phase_error
			break;
		}

		nco_crcf_pll_step(l_nco, -phase_error);

		/* Clamp the PLL frequency between min and max */
		float freq = nco_crcf_get_frequency(l_nco);
		if (freq > freq_max)
			nco_crcf_set_frequency(l_nco, freq_max);
		if (freq < freq_min)
			nco_crcf_set_frequency(l_nco, freq_min);


		resamp_crcf_execute(l_resamp, s, samples2, &nsamp2);

		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++) {
			Sample s2 = samples2[si2];

			/* AGC */
			agc_crcf_execute(l_agc, s2, &s2);


			/* Run symbols sync */
			Complex synced_symbols[4];
			unsigned int output_symbols = 0;
			symsync_crcf_execute(l_sync, &s2, 1, synced_symbols, &output_symbols);
			//symsync_crcf_step(l_sync, s2, synced_symbols, &output_symbols);

			Sample s3 = synced_symbols[0];
			


		}
		timestamp += sample_ns;
	}
}


void PSKDemodulator::lockReceiver(bool locked, Timestamp now) {
	receiver_lock = locked;
#if 0
	if (locked) {
		setMetadata.emit("cfo", nco_crcf_get_frequency(l_nco) / nco_1Hz);
		setMetadata.emit("rssi", agc_crcf_get_rssi(l_agc));

		// Sync acquired
		symsync_crcf_set_lf_bw(l_sync, conf.symsync_bandwidth1);
		nco_crcf_pll_set_bandwidth(l_nco, conf.pll_bandwidth1);
		agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth1);
	}
	else {
		// Sync lost
		symsync_crcf_set_lf_bw(l_sync, conf.symsync_bandwidth0);
		nco_crcf_pll_set_bandwidth(l_nco, conf.pll_bandwidth0);
		agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth0);
	}
#endif
}

void PSKDemodulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
	conf_dirty = true;
}


Block* createPSKDemodulator(const Kwargs &args)
{
	return new PSKDemodulator();
}

static Registry registerPSKDemodulator("PSKDemodulator", &createPSKDemodulator);
