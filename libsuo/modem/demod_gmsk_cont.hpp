#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>


namespace suo {

/*
 * GMSK demodulator
 */
class GMSKContinousDemodulator : public Block
{
public:

	/* Configuration struct for the FSK demod */
	struct Config {
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
		 * Frequency offset from the center frequency (Hz)
		 */
		float frequency_offset;

		/*
		 * Number of samples per symbol/bit after decimation.
		 */
		unsigned int samples_per_symbol;
		unsigned int filter_delay;
		/*
		 * Gaussian filter bandwidth-time product / excess bandwidth factor.
		 * Usually 0.3 or 0.5
		 */
		float bt;

		/* Hz */
		float pll_bandwidth0;
		float pll_bandwidth1;

		float agc_bandwidth0;
		float agc_bandwidth1;

		float symsync_bandwidth0;
		float symsync_bandwidth1;
	};

	GMSKContinousDemodulator(const Config& conf = Config());
	~GMSKContinousDemodulator();

	void reset();

	void sinkSamples(const SampleVector& samples, Timestamp timestamp);
	void lockReceiver(bool locked, Timestamp now);

	Port<Symbol, Timestamp> sinkSymbol;
	Port<SoftSymbol, Timestamp> sinkSoftSymbol;

	void setFrequencyOffset(float frequency_offset);


private:
	void update_nco();

	/* Configuration */
	Config conf;
	bool conf_dirty;

	// float resamprate;
	Timestamp sample_ns;
	unsigned resampint;
	float nco_1Hz;
	float afc_speed;
	bool receiver_lock;

	Sample x_prime;
	float freq_min, freq_max;

	float k_ref;

	/* liquid-dsp objects */
	nco_crcf l_nco;
	resamp_crcf l_resamp;
	agc_crcf l_agc;
	agc_crcf l_bg_agc;
	firfilt_rrrf l_filter;
	float k_inv;
	symsync_rrrf l_symsync;
	windowcf l_demod_delay;

	/* Buffers */
	Frame* frame;

};

}; // namespace suo

