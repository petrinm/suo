
#include <cassert>
#include <iostream> // cout, cerr
#include <algorithm> // clamp

#include "modem/demod_gmsk_cont.hpp"
#include "registry.hpp"


using namespace std;
using namespace suo;

#define FRAMELEN_MAX 0x900



GMSKContinousDemodulator::Config::Config() {
	sample_rate = 1e6;
	symbol_rate = 9600;
	center_frequency = 10e3;
	frequency_offset = 0.0f;
	samples_per_symbol = 4;
	filter_delay = 5;
	bt = 0.5;

	pll_bandwidth0 = 4e-7f;
	pll_bandwidth1 = 4e-7f;

	agc_bandwidth0 = 9e-2f;
	agc_bandwidth1 = 2e-2f;

	symsync_bandwidth0 = 0.01f;
	symsync_bandwidth1 = 0.01f;
}

GMSKContinousDemodulator::GMSKContinousDemodulator(const Config& conf) :
	conf(conf)
{


	// Carson bandwidth rule: Bandwidth = 2 * (deviation + symbol_rate)  without Gaussian filter
	float signal_bandwidth = 2 * (0.5 * conf.symbol_rate + conf.symbol_rate); // [Hz]
	if ((abs(conf.center_frequency) + 0.5 * signal_bandwidth) / conf.sample_rate > 0.50)
		throw SuoError("GMSKContinousDemodulator: Center frequency too large for given sample rate!");


	/* Configure a resampler for a fixed oversampling ratio */
	float resamprate = conf.symbol_rate * conf.samples_per_symbol / conf.sample_rate;
	double bw = 0.75 * resamprate / conf.samples_per_symbol;
	int semilen = lroundf(1.0f / bw);
	l_resamp = resamp_crcf_create(resamprate, semilen, bw, 60.0f, 16);

	/* Calculate maximum number of output samples after feeding one sample
	 * to the resampler. This is needed to allocate a big enough array. */
	resampint = ceil(1 / resamprate);
	sample_ns = round(1.0e9 / conf.sample_rate);

	/* 
	 * NCO:
	 * Limit AFC range to half of symbol rate to keep it from wandering too far
	 */
	nco_1Hz = pi2f / conf.sample_rate;
	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_pll_set_bandwidth(l_nco, conf.pll_bandwidth0 * nco_1Hz);
	update_nco();


	/*
	 * AGC: Automatic gain control
	 */
	l_agc = agc_crcf_create();
	agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth0 / conf.samples_per_symbol);
	//agc_crcf_set_gain_limits(l_agc, 1e-2f, 1e2f);
	l_bg_agc = agc_crcf_create();
	agc_crcf_set_bandwidth(l_bg_agc, 2e-3f / conf.samples_per_symbol);


	float kf = 0.01 * conf.samples_per_symbol;
	k_ref = 1.0f / (2 * M_PI * kf);

	/* 
	 * Symbol synchronizer:
	 * - Undistors the gaussian filtering
	 * - Runs polyphase filter bank for symbol timing recovery 
	 * - Decimates to symbol rate
	 */
	l_symsync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_GMSKRX, conf.samples_per_symbol, conf.filter_delay, conf.bt, 16);
	symsync_rrrf_set_lf_bw(l_symsync, conf.symsync_bandwidth0 / conf.samples_per_symbol);

	reset();
}


GMSKContinousDemodulator::~GMSKContinousDemodulator()
{
	agc_crcf_destroy(l_agc);
	symsync_rrrf_destroy(l_symsync);
}


void GMSKContinousDemodulator::reset() {
	x_prime = 0.0f;
	nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset));
	receiver_lock = false;
}


void GMSKContinousDemodulator::update_nco()
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


void GMSKContinousDemodulator::sinkSamples(const SampleVector& samples, Timestamp now)
{

	/* Allocate small buffers from stack */
	Sample samples2[resampint];

	if (conf_dirty && receiver_lock == false)
		update_nco();
	
	float d_hat;
	size_t symbol_phase = 0;
	Sample null;


	for (size_t si = 0; si < samples.size(); si++) {
		unsigned nsamp2 = 0, si2;
		Sample s = samples[si];


		/* Downconvert and resample one input sample at a time */
		nco_crcf_step(l_nco);
		nco_crcf_mix_down(l_nco, s, &s);
		resamp_crcf_execute(l_resamp, s, samples2, &nsamp2);

		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++) {
			Sample s2 = samples2[si2];

			/* Update AGC just to track RSSI's */
			agc_crcf_execute(l_bg_agc, s2, &null);
			agc_crcf_execute(l_agc, s2, &null);

#if 1
			/*  */
			float phase_error = arg(conj(x_primee) * s2) * k_ref;
			x_primee = s2;
			nco_crcf_mix_up(l_nco, x_primee, &x_primee);
			nco_crcf_pll_step(l_nco, phase_error);
			nco_crcf_mix_down(l_nco, x_primee, &x_primee);
#else
			/* Update PLL feedback (Costas loop for QPSK) */
			//float phase_error = copysign(s2.imag(), s2.real()) - copysign(s2.real(), s2.imag());
			//float phase_error = s2.real() * s2.imag();
			float phase_error = (s2 * conj(x_primee)).imag();
			x_primee = s2;
			nco_crcf_pll_step(l_nco, -100* phase_error);
#endif

			/* Clamp the PLL frequency between min and max */
			float freq = nco_crcf_get_frequency(l_nco);
			if (freq > freq_max)
				nco_crcf_set_frequency(l_nco, freq_max);
			if (freq < freq_min)
				nco_crcf_set_frequency(l_nco, freq_min);


			/* Quadrature FM-demodulation */
			float phi = arg(conj(x_prime) * s2) * k_ref;
			x_prime = s2;

			/* Run symbols synchronization.
			 * This has also gaussian undistortion and decimation */
			float synced_symbol;
			unsigned int output_symbols = 0;
			symsync_rrrf_execute(l_symsync, &phi, 1, &synced_symbol, &output_symbols);

			if (output_symbols == 0)
				continue;

			assert(output_symbols == 1);

		
			/* Process one output symbol from synchronizer */
			Timestamp symbol_time = now + si * sample_ns;
			//now -= conf.filter_delay * resampint; // or / resamprate

			Symbol decision = (synced_symbol >= 0) ? 1 : 0;
			//cout << (int)decision << " ";
			sinkSymbol.emit(decision, symbol_time);

			//SoftSymbol soft_bit = synced_symbol;
			//sinkSoftSymbol.emit

#if 0
			//SinkSymbol(decision, timestamp)) {
			if (0) { 
				receiver_lock = true;
			}
			else {
				/* No sync, no lock */
				receiver_lock = false;
			}
#endif

		}
	}
}

void GMSKContinousDemodulator::lockReceiver(bool locked, Timestamp now) {
	receiver_lock = locked;
	if (locked) {
		
		setMetadata.emit("cfo", nco_crcf_get_frequency(l_nco) / nco_1Hz);
		setMetadata.emit("rssi", agc_crcf_get_rssi(l_agc));
		setMetadata.emit("bg_rssi", agc_crcf_get_rssi(l_bg_agc));

		symsync_rrrf_set_lf_bw(l_symsync, conf.symsync_bandwidth1 / conf.samples_per_symbol);
		nco_crcf_pll_set_bandwidth(l_nco, conf.pll_bandwidth1 * nco_1Hz);
		agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth1 / conf.samples_per_symbol);
		agc_crcf_lock(l_bg_agc);
	}
	else {
		symsync_rrrf_set_lf_bw(l_symsync, conf.symsync_bandwidth0 / conf.samples_per_symbol);
		nco_crcf_pll_set_bandwidth(l_nco, conf.pll_bandwidth0 * nco_1Hz);
		agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth0 / conf.samples_per_symbol);
		agc_crcf_unlock(l_bg_agc);
	}
}


void GMSKContinousDemodulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
	conf_dirty = true;
}


Block* createGMSKContinousDemodulator(const Kwargs &args)
{
	return new GMSKContinousDemodulator();
}

static Registry registerGMSKContinousDemodulator("GMSKContinousDemodulator", &createGMSKContinousDemodulator);
