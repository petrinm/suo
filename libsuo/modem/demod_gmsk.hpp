#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>

namespace suo {

/*
 *
 */
class GMSKDemodulator : public Block
{
public:

	enum State {
		STATE_DETECTFRAME = 0, // detect frame (seek p/n sequence)
		STATE_RXPREAMBLE,	   // receive p/n sequence
		STATE_RXPAYLOAD,	   // receive payload data
		STATE_HELLO,
	};
	
	/* Configuration struct for the GMSK demod block */
	struct Config
	{
		Config();

		/*
		 * Input IQ sample rate as samples per second.
		 */
		float sample_rate;

		/*
		 * Symbol rate as symbols per second
		 */
		float symbol_rate;

		/*
		 * Signal center frequency as Hz
		 */
		float center_frequency;

		/*
		 */
		unsigned int samples_per_symbol;

		/*
		 * Gaussian filter bandwidth-time product / excess bandwidth factor.
		 * Usually 0.3 or 0.5.
		 */
		float bt;

		unsigned int sync_word;
		unsigned int sync_len;
	};

	explicit GMSKDemodulator(const Config& args = Config());
	~GMSKDemodulator();

	GMSKDemodulator(const GMSKDemodulator&) = delete;
	GMSKDemodulator& operator=(const GMSKDemodulator&) = delete;

	void sinkSamples(SampleVector samples, Timestamp timestamp);
	void reset();

	Port<Symbol, Timestamp> SymbolSink;

	void setFrequencyOffset(float frequency_offset);

private:

	int update_symbol_sync(float _x, float *_y);
	void pushpn();

	void update_fi(Complex x);
	void execute_detectframe(const Complex x);
	void execute_rxpreamble(const Complex x);
	void execute_rxpayload(const Complex x);
	void execute_sample(const Complex x);

	/* Configuration */
	Config conf;

	// float resamprate;
	unsigned int resampint;
	uint64_t syncmask;

	unsigned int m; // filter semi-length (symbols)

	/* Deframer state */
	State state;
	uint64_t latest_bits;
	unsigned framepos, totalbits;
	bool receiving_frame;

	unsigned int mod_rate;

	float nco_1Hz;
	float center_frequency; // Currently set center frequency

	/* liquid-dsp objects */
	nco_crcf l_nco;
	resamp_crcf l_resamp;
	gmskdem l_demod;


	float fi_hat;
	Complex x_prime;

	// timing recovery objects, states
	firpfb_rrrf l_mf;  // matched filter decimator
	firpfb_rrrf l_dmf; // derivative matched filter decimator
	unsigned int npfb; // number of filters in symsync
	float pfb_q;	   // filtered timing error
	float pfb_soft;	   // soft filterbank index
	int pfb_index;	   // hard filterbank index
	int pfb_timer;	   // filterbank output flag
	float symsync_out; // symbol synchronizer output

	// synchronizer objects
	detector_cccf l_frame_detector; // pre-demod detector
	float tau_hat;					// fractional timing offset estimate
	float dphi_hat;					// carrier frequency offset estimate
	float gamma_hat;				// channel gain estimate
	windowcf l_buffer;				// pre-demod buffered samples, size: k*(pn_len+m)
	nco_crcf l_nco_coarse;			// coarse carrier frequency recovery

	unsigned int preamble_counter;
	float *preamble_pn; // preamble p/n sequence (known)
	float *preamble_rx; // preamble p/n sequence (received)

	unsigned int preamble_len;

	/* Buffers */
	Frame* frame;

};

};
