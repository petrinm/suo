
#include <cassert>
#include <iostream> // cout, cerr
#include <algorithm> // clamp

#include "modem/demod_gmsk_cont.hpp"
#include "registry.hpp"
#include "plotter.hpp"

#include <matplot/matplot.h>

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
	bt = 0.3;

	pll_bandwidth0 = 0.06f;
	pll_bandwidth1 = 0.005f;

	agc_bandwidth0 = 0.001f;
	agc_bandwidth1 = 0.001f;

	symsync_bandwidth0 = 0.1f;
	symsync_bandwidth1 = 0.1f;

}

GMSKContinousDemodulator::GMSKContinousDemodulator(const Config& conf) :
	conf(conf)
{

	/* Configure a resampler for a fixed oversampling ratio */
	float resamprate = conf.symbol_rate * conf.samples_per_symbol / conf.sample_rate;
	double bw = 0.4 * resamprate / conf.samples_per_symbol;
	int semilen = lroundf(1.0f / bw);
	unsigned int npfb = 32;
	cerr << "resamprate: " << resamprate << endl;
	cerr << "bw: " << bw << endl;
	cerr << "semilen: " << semilen << endl;

	l_resamp = resamp_crcf_create(resamprate, semilen, bw, 60.0f, npfb);

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
	agc_crcf_set_bandwidth(l_agc, 5e-3f); // TODO: /= conf.samples_per_symbol
	//agc_crcf_set_gain_limits(l_agc, 1e-2f, 1e2f);

	l_bg_agc = agc_crcf_create();
	agc_crcf_set_bandwidth(l_bg_agc, 1e-6f / conf.samples_per_symbol);


	l_demod_delay = windowcf_create(4 * conf.samples_per_symbol);

	float kf = 0.01 * conf.samples_per_symbol;
	k_ref = 1.0f / (2 * M_PI * kf);

	/* Symbol synchroniser */
	unsigned int m = 5;                 // filter delay (symbols)
	float        beta = 0.5f;           // filter excess bandwidth factor
	unsigned int num_filters = 32;      // number of filters in the bank
	unsigned int num_symbols = 400;     // number of data symbols


	//l_symsync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_ARKAISER, conf.samples_per_symbol, m, beta, num_filters);
	l_symsync = symsync_rrrf_create_kaiser(conf.samples_per_symbol, 8, 0 /* ignored */, 64);
	symsync_rrrf_set_lf_bw(l_symsync, conf.symsync_bandwidth0 / conf.samples_per_symbol);

	//symsync_rrrf_set_output_rate(l_symsync, conf.samples_per_symbol); // Disable decimation

#if 0
	/* Compute filter coefficients */
	size_t h_len = 2 * conf.samples_per_symbol * conf.filter_delay + 1;
	float h[h_len];
	liquid_firdes_gmskrx(conf.samples_per_symbol, conf.filter_delay, conf.bt, 0.0f, h);
	l_filter = firfilt_rrrf_create(h, h_len);
#else
	l_filter = firfilt_rrrf_create_rnyquist(LIQUID_FIRFILT_GMSKRX, conf.samples_per_symbol, 8, conf.bt, 0);
	firfilt_rrrf_set_scale(l_filter, 1.f / (float)conf.samples_per_symbol);
#endif

	reset();
}


GMSKContinousDemodulator::~GMSKContinousDemodulator()
{
	agc_crcf_destroy(l_agc);
	firfilt_rrrf_destroy(l_filter);
	symsync_rrrf_destroy(l_symsync);
	windowcf_destroy(l_demod_delay);
}


void GMSKContinousDemodulator::reset() {
	x_prime = 0.0f;
	//filter_rrrf_reset(l_filter);
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


	Plotter plot_a(200);
	Plotter plot_b(200);
	ComplexPlotter iq_plot1(1024);
	iq_plot1.modulo = conf.samples_per_symbol;
	iq_plot1.offset = 0;

	ComplexPlotter iq_plot2(200);
	iq_plot2.modulo = conf.samples_per_symbol;
	iq_plot2.offset = 0;
	iq_plot2.auto_clear = true;



	for (size_t si = 0; si < samples.size(); si++) {
		unsigned nsamp2 = 0, si2;
		Sample s = samples[si];

		if (0 && iq_plot1.push(s)) {
			iq_plot1.plot_fft();
			matplot::show();
			return;
		}

		/* Downconvert and resample one input sample at a time */
		nco_crcf_step(l_nco);
		nco_crcf_mix_down(l_nco, s, &s);
		resamp_crcf_execute(l_resamp, s, samples2, &nsamp2);


		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++) {
			Sample s2 = samples2[si2];

			/* Update AGC */
			agc_crcf_execute(l_bg_agc, s2, &null);
			agc_crcf_execute(l_agc, s2, &null);
			//agc_crcf_execute(l_agc, s2, &s2);

#if 1
			/* Update PLL feedback (Costas loop for QPSK) */
			float phase_error = copysign(s2.imag(), s2.real()) - copysign(s2.real(), s2.imag());
#else
			float phase_error = s2.real() * s2.imag();
#endif
			nco_crcf_pll_step(l_nco, phase_error);

			/* Clamp the PLL frequency between min and max */
			float freq = nco_crcf_get_frequency(l_nco);
			if (freq > freq_max)
				nco_crcf_set_frequency(l_nco, freq_max);
			if (freq < freq_min)
				nco_crcf_set_frequency(l_nco, freq_min);


			if (receiver_lock) {
				if (0 && plot_a.push(freq / nco_1Hz)) {
					plot_a.plot();
					matplot::show();
					return;
				}
				if (0 && iq_plot2.push(s2)) {
					iq_plot2.plot_constellation();
					matplot::show();
					return;
				}
			}

			/* Quadrature FM-demodulation */
			float phi = arg(conj(x_prime) * s2) * k_ref;
			x_prime = s2;

			/* */
			firfilt_rrrf_push(l_filter, phi);
			firfilt_rrrf_execute(l_filter, &phi);
			if (0 && iq_plot2.push(s2)) {
				iq_plot2.plot_constellation();
				matplot::show();
				return;
			}

			/* Run symbols sync */
			float synced_symbols[4];
			unsigned int output_symbols = 0;
			symsync_rrrf_execute(l_symsync, &phi, 1, synced_symbols, &output_symbols);

#if 0
			for (size_t j = 0; j < output_symbols; j++) {
				plot2.push_back(synced_symbols[j]);
				if (plot2.size() == plot2.capacity()) {
					matplot::figure();
					matplot::hold(matplot::on);
					matplot::plot(plot1);
					matplot::plot(plot2);
					matplot::show();
					return;
				}
			}
#endif

			assert(output_symbols <= 1);
			if (output_symbols == 0)
				continue;


			float synced_symbol = synced_symbols[0];
			//cout << "synced_symbol " << synced_symbol << endl;
#if 0
			if (1 && plot_b.push(synced_symbol)) {
				plot_b.plot();
				matplot::show();
				return;
			}
#endif

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


			/*
			frame->setMetadata("cfo", nco_crcf_get_frequency(l_nco) / nco_1Hz);
			frame->setMetadata("rssi", agc_crcf_get_rssi(l_agc));
			frame->setMetadata("bg_rssi", agc_crcf_get_rssi(l_bg_agc));
			*/


		}
	}
}

void GMSKContinousDemodulator::lockReceiver(bool locked, Timestamp now) {
	receiver_lock = locked;
	if (locked) {

		cout << "cfo: " << (nco_crcf_get_frequency(l_nco) / nco_1Hz) << endl;
		cout << "rssi: " << agc_crcf_get_rssi(l_agc) << endl;
		cout << "bg_rssi: " << agc_crcf_get_rssi(l_bg_agc) << endl;

		symsync_rrrf_set_lf_bw(l_symsync, conf.symsync_bandwidth1 / conf.samples_per_symbol);
		agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth1);
		agc_crcf_lock(l_bg_agc);
	}
	else {
		symsync_rrrf_set_lf_bw(l_symsync, conf.symsync_bandwidth0 / conf.samples_per_symbol);
		agc_crcf_set_bandwidth(l_agc, conf.agc_bandwidth0);
		agc_crcf_lock(l_bg_agc);
	}
}


void GMSKContinousDemodulator::setFrequencyOffset(float frequency_offset) {
	cerr << "Downlink frequency offset " << frequency_offset;
	conf.frequency_offset = frequency_offset;
	conf_dirty = true;
}


Block* createGMSKContinousDemodulator(const Kwargs &args)
{
	return new GMSKContinousDemodulator();
}

static Registry registerGMSKContinousDemodulator("GMSKContinousDemodulator", &createGMSKContinousDemodulator);
