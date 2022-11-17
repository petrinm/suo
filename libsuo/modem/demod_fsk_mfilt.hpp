#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>

namespace suo {

/*
 * (G)FSK demodulator implementation using matched filters.
 */
class FSKMatchedFilterDemodulator : public Block
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

		/* Modulation index */
		float modindex;

		/* Frequency deviation */
		float deviation;

		/*
		 * Number of samples per symbol/bit after decimation.
		 */
		unsigned int samples_per_symbol;

		float bt;

		/*
		 * Number of bit in one symbol (symbol complexity)
		 */
		unsigned bits_per_symbol;

		/*
		 * Signal center frequency as Hz
		 */
		float center_frequency;

		/* 
		 * Frequency offset from the center frequency (Hz)
		 */
		float frequency_offset;

		/* */
		float pll_bandwidth0;
		float pll_bandwidth1;
	};

	explicit FSKMatchedFilterDemodulator(const Config& conf = Config());
	~FSKMatchedFilterDemodulator();

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
	unsigned int constellation_size;

	/* AFC state */
	float freq_min; // Normalized minimum frequency
	float freq_max; // Normalized maximum frequency

	/* Timing synchronizer state */
	float ss_comb[4];
	float demod_prev;
	unsigned ss_p, ss_ps;


	agc_crcf l_bg_agc;

	/* General metadata */
	float est_power; // Running estimate of the signal power

	/* liquid-dsp objects */
	nco_crcf l_nco;
	resamp_crcf l_resamp;
	std::vector<firfilt_cccf> matched_filters;
	firfilt_rrrf l_eqfir;
	windowcf l_sync_window;
	symsync_rrrf l_symsync;

	/* Buffers */
	Frame frame;

};

}; // namespace suo

