#include "demod_gmsk.hpp"
#include "registry.hpp"

#include <string>
#include <assert.h>
#include <iostream>
#include <liquid/liquid.h>
#include <iomanip>

using namespace std;
using namespace suo;

/*
 * Higly influenced by:
 *   https://github.com/jgaeddert/liquid-dsp/blob/master/src/framing/src/gmskframesync.c
 *   https://github.com/jgaeddert/liquid-dsp/blob/master/src/modem/src/gmskdem.c
*/

#define FRAMELEN_MAX 0x900


GMSKDemodulator::Config::Config() {
	sample_rate = 1e6;
	symbol_rate = 9600;
	center_frequency = 100000;
	bt = 0.3;
	samples_per_symbol = 4;
}


GMSKDemodulator::GMSKDemodulator(const Config& conf) :
	conf(conf)
{


	// Carson bandwidth rule: Bandwidth = 2 * (deviation + symbol_rate)
	float signal_bandwidth = 2 * (0.5 * conf.symbol_rate + conf.symbol_rate); // [Hz]
	if ((abs(conf.center_frequency) + 0.5 * signal_bandwidth) / conf.sample_rate > 0.50)
		throw SuoError("GMSKDemodulator: Center frequency too large for given sample rate!");

	//syncmask = (1ULL << conf.synclen) - 1;
	//framepos = conf.framelen;

	/* Configure a resampler for a fixed oversampling ratio */
	float resamprate = conf.symbol_rate * conf.samples_per_symbol / conf.sample_rate;
	double bw = 0.4 * resamprate / conf.samples_per_symbol;
	int semilen = lroundf(1.0f / bw);
	l_resamp = resamp_crcf_create(resamprate, 25, 0.4f / conf.samples_per_symbol, 60.0f, 32);
	/* Calculate maximum number of output samples after feeding one sample
	 * to the resampler. This is needed to allocate a big enough array. */
	resampint = ceilf(1 / resamprate);


	center_frequency = conf.center_frequency;
	nco_1Hz = pi2f / conf.sample_rate;

	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_pll_set_bandwidth(l_nco, 0.5 * nco_1Hz);
	nco_crcf_set_frequency(l_nco, nco_1Hz * center_frequency);


	/*
	 * Frame detector
	 */
	preamble_len = conf.sync_len;
	preamble_pn = new float[preamble_len];
	preamble_rx = new float[preamble_len];
	Sample syncword_samples[preamble_len * conf.samples_per_symbol];

	gmskmod mod = gmskmod_create(conf.samples_per_symbol, m, conf.bt);

	for (unsigned int i=0; i<conf.sync_len + m; i++) {

		unsigned char bit;
		if (i < m)
			bit = (conf.sync_word & (conf.sync_len - i)) != 0;
		else
			bit = i & 1;

		// save p/n sequence
		if (i < preamble_len)
			preamble_pn[i] = bit ? 1.0f : -1.0f;

		// modulate/interpolate
		if (i < m)
			gmskmod_modulate(mod, bit, &syncword_samples[0]);
		else
			gmskmod_modulate(mod, bit, &syncword_samples[(i - m) * conf.samples_per_symbol]);
	}
	gmskmod_destroy(mod);


	// create frame detector
	float threshold = 0.50f;     // detection threshold
	float dphi_max  = 0.05f;    // maximum carrier offset allowable
	l_frame_detector = detector_cccf_create(syncword_samples, preamble_len * conf.samples_per_symbol, threshold, dphi_max);
	l_buffer = windowcf_create(conf.samples_per_symbol * (preamble_len + m));

	// create symbol timing recovery filters
	npfb = 32;   // number of filters in the bank
	l_mf   = firpfb_rrrf_create_rnyquist( LIQUID_FIRFILT_GMSKRX, npfb, conf.samples_per_symbol, m, conf.bt);
	l_dmf  = firpfb_rrrf_create_drnyquist(LIQUID_FIRFILT_GMSKRX, npfb, conf.samples_per_symbol, m, conf.bt);

	// create down-coverters for carrier phase tracking
	l_nco_coarse = nco_crcf_create(LIQUID_NCO);

	state = STATE_DETECTFRAME;
	reset();
}

void GMSKDemodulator::reset()
{
	state = STATE_DETECTFRAME;

	windowcf_reset(l_buffer);
	detector_cccf_reset(l_frame_detector);
	nco_crcf_reset(l_nco_coarse);

	preamble_counter = 0;

	x_prime = 0.0f;
	fi_hat  = 0.0f;

	firpfb_rrrf_reset(l_mf);
	firpfb_rrrf_reset(l_dmf);
	pfb_q = 0.0f;
}


GMSKDemodulator::~GMSKDemodulator()
{
	//iirfilt_crcf_destroy(l_prefilter);        // pre-demodulator filter
	firpfb_rrrf_destroy(l_mf);                // matched filter
	firpfb_rrrf_destroy(l_dmf);               // derivative matched filter
	nco_crcf_destroy(l_nco_coarse);           // coarse NCO

	// preamble
	detector_cccf_destroy(l_frame_detector);
	windowcf_destroy(l_buffer);

	delete[] preamble_pn;
	delete[] preamble_rx;
}



// update symbol synchronizer internal state (filtered error, index, etc.)
//  _q      :   frame synchronizer
//  _x      :   input sample
//  _y      :   output symbol
int GMSKDemodulator::update_symbol_sync(float _x, float *_y)
{
	// push sample into filterbanks
	firpfb_rrrf_push(l_mf,  _x);
	firpfb_rrrf_push(l_dmf, _x);

	//
	float mf_out  = 0.0f;    // matched-filter output
	float dmf_out = 0.0f;    // derivatived matched-filter output


	int sample_available = 0;

	// compute output if timeout
	if (pfb_timer <= 0) {
		sample_available = 1;

		// reset timer
		pfb_timer = conf.samples_per_symbol; // samples/symbol

		firpfb_rrrf_execute(l_mf,  pfb_index, &mf_out);
		firpfb_rrrf_execute(l_dmf, pfb_index, &dmf_out);

		// update filtered timing error
		// lo  bandwidth parameters: {0.92, 1.20}, about 100 symbols settling time
		// med bandwidth parameters: {0.98, 0.20}, about 200 symbols settling time
		// hi  bandwidth parameters: {0.99, 0.05}, about 500 symbols settling time
		pfb_q = 0.99f*pfb_q + 0.05f * (conj(mf_out) * dmf_out).real();

		// accumulate error into soft filterbank value
		pfb_soft += pfb_q;

		// compute actual filterbank index
		pfb_index = roundf(pfb_soft);

		// contrain index to be in [0, npfb-1]
		while (pfb_index < 0) {
			pfb_index += npfb;
			pfb_soft  += npfb;

			// adjust pfb output timer
			pfb_timer--;
		}
		while (pfb_index > (int)npfb - 1) {
			pfb_index -= npfb;
			pfb_soft  -= npfb;

			// adjust pfb output timer
			pfb_timer++;
		}
		// cout << "  b/soft    :   " << pfb_soft << endl; // %12.8f\n;
	}

	// decrement symbol timer
	pfb_timer--;

	// set output and return
	*_y = mf_out / (float)(conf.samples_per_symbol);

	return sample_available;
}



// push buffered p/n sequence through synchronizer
void GMSKDemodulator::pushpn()
{
	unsigned int i;

	// reset filterbanks
	firpfb_rrrf_reset(l_mf);
	firpfb_rrrf_reset(l_dmf);

	// read buffer
	Complex * rc;
	windowcf_read(l_buffer, &rc);

	// compute delay and filterbank index
	//  tau_hat < 0 :   delay = 2*k*m-1, index = round(   tau_hat *npfb), flag = 0
	//  tau_hat > 0 :   delay = 2*k*m-2, index = round((1-tau_hat)*npfb), flag = 0
	assert(tau_hat < 0.5f && tau_hat > -0.5f);
	unsigned int delay = 2 * conf.samples_per_symbol * m - 1; // samples to buffer before computing output
	pfb_soft       = -tau_hat*npfb;
	pfb_index      = (int) roundf(pfb_soft);
	while (pfb_index < 0) {
		delay         -= 1;
		pfb_index += npfb;
		pfb_soft  += npfb;
	}
	pfb_timer = 0;

	// set coarse carrier frequency offset
	nco_crcf_set_frequency(l_nco_coarse, dphi_hat);

	unsigned int buffer_len = (preamble_len + m) * conf.samples_per_symbol;
	for (i=0; i<delay; i++) {
		Complex y;
		nco_crcf_mix_down(l_nco_coarse, rc[i], &y);
		nco_crcf_step(l_nco_coarse);

		// update instantanenous frequency estimate
		update_fi(y);

		// push initial samples into filterbanks
		firpfb_rrrf_push(l_mf,  fi_hat);
		firpfb_rrrf_push(l_dmf, fi_hat);
	}

	// set state (still need a few more samples before entire p/n
	// sequence has been received)
	state = STATE_RXPREAMBLE;

	for (i=delay; i<buffer_len; i++) {
		// run remaining samples through sample state machine
		execute_sample(rc[i]);
	}

}


/*
 * Update instantaneous frequency estimate
 */
void GMSKDemodulator::update_fi(Complex _x)
{
	// compute differential phase
	fi_hat = arg(conj(x_prime) * _x) * conf.samples_per_symbol;

	// update internal state
	x_prime = _x;
}


void GMSKDemodulator::execute_detectframe(const Complex _x)
{
	// push sample into pre-demod p/n sequence buffer
	windowcf_push(l_buffer, _x);

	// push through pre-demod synchronizer
	int detected = detector_cccf_correlate(l_frame_detector, _x,
		&tau_hat,  &dphi_hat, &gamma_hat);

	// check if frame has been detected
	if (detected) {
#if 1
		cout << endl << endl;
		cout << "***** Frame detected!"; // %8.4f, dphi-hat:%8.4f, gamma:%8.2f dB\n",
		cout << "tau-hat: " << setw(8) << setprecision(4) << tau_hat;
		cout << "dphi-hat: " << dphi_hat;
		cout << "gamma: " << setprecision(2) << (20 * log10f(gamma_hat)) << " dB" << endl,
#endif
		// push buffered samples through synchronizer
		// NOTE: state will be updated to STATE_RXPREAMBLE internally
		pushpn();
	}
}


void GMSKDemodulator::execute_rxpreamble(const Complex _x)
{
	if (preamble_counter == preamble_len) {
		reset();
		throw SuoError("Preample fault");
	}

	// mix signal down
	Complex y;
	nco_crcf_mix_down(l_nco_coarse, _x, &y);
	nco_crcf_step(l_nco_coarse);

	// update instantanenous frequency estimate
	update_fi(y);

	// update symbol synchronizer
	float mf_out = 0.0f;
	int sample_available = update_symbol_sync(fi_hat, &mf_out);

	// compute output if timeout
	if (sample_available) {
		// save output in p/n symbols buffer
		preamble_rx[preamble_counter] = mf_out / (float)conf.samples_per_symbol;

		unsigned char s = mf_out > 0.0f ? 1 : 0;
		cout << s << " ";

		// update counter
		preamble_counter++;

		if (preamble_counter == preamble_len) {
			//syncpn();
			cout << endl;
			state = STATE_RXPAYLOAD;
		}
	}
}

void GMSKDemodulator::execute_rxpayload(const Complex _x)
{
	// mix signal down
	Complex y;
	nco_crcf_mix_down(l_nco_coarse, _x, &y);
	nco_crcf_step(l_nco_coarse);

	// update instantanenous frequency estimate
	update_fi(y);

	cout << "x ";

	// update symbol synchronizer
	float mf_out = 0.0f;
	int sample_available = update_symbol_sync(fi_hat, &mf_out);

	// compute output if timeout
	if (sample_available) {
		// demodulate
		unsigned char s = mf_out > 0.0f ? 1 : 0;
		cout << s << " ";

		if (state == STATE_RXPREAMBLE) {
			if (0) { // SymbolSink(s, 0) == 1) {
				cout << "PRKL\n";
				state = STATE_HELLO;
			}
		}
		else {
			if (0) { // SymbolSink(s, 0) == 0) {
				cout << "reset\n";
				reset();
			}
		}
	}
}


void GMSKDemodulator::execute_sample(Complex _x)
{
	switch (state) {
	case STATE_DETECTFRAME: return execute_detectframe(_x);
	case STATE_RXPREAMBLE:  return execute_rxpreamble (_x);
	case STATE_RXPAYLOAD:   return execute_rxpayload  (_x);
	case STATE_HELLO:  return execute_rxpayload  (_x);
	default:;
		throw SuoError("GMSK fault state");
	}
}


void GMSKDemodulator::sinkSamples(const SampleVector& samples, Timestamp timestamp)
{
	/* Allocate small buffers from stack */
	Sample samples2[resampint];

	Timestamp sample_ns = roundf(1.0e9f / conf.sample_rate);

	size_t si;
	for(si = 0; si < samples.size(); si++) {
		unsigned nsamp2 = 0, si2;
		Sample s = samples[si];

		/* Downconvert and resample one input sample at a time */
		nco_crcf_step(l_nco);
		nco_crcf_mix_down(l_nco, s, &s);
		resamp_crcf_execute(l_resamp, s, samples2, &nsamp2);
		assert(nsamp2 <= resampint);

		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++)
			execute_sample(samples2[si2]);

		timestamp += sample_ns;
	}

}


Block* createGMSKDemodulator(const Kwargs &args)
{
	return new GMSKDemodulator();
}

static Registry registerGMSKDemodulator("GMSKDemodulator", &createGMSKDemodulator);