#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>

namespace suo {

class FSKModulator : public Block
{
	/*
	 * n-FSK modulator
	 */
public:

	enum State {
		Idle,
		Waiting,
		Transmitting,
		Trailer
	};

	struct Config {
		Config();

		/* Output sample rate as samples per second*/
		float sample_rate;

		/* Symbol rate as symbols per second */
		float symbol_rate;

		/* Number of bit in one symbol (symbol complexity) */
		unsigned int complexity;

		/* Center frequency */
		float center_frequency;

		/* */
		float frequency_offset;

		/* Modulation index */
		float modindex;

		/* Frequency deviation */
		float deviation;

		/* Gaussian filter bandwidth-symbol time product */
		float bt;

		/* Length of the start/stop ramp in symbols */
		unsigned int ramp_up_duration, ramp_down_duration;
	};


	explicit FSKModulator(const Config& conf = Config());

	~FSKModulator();

	void reset();

	void sourceSamples(SampleVector samples, Timestamp timestamp);
	
	Port<SymbolVector&, Timestamp> sourceSymbols;
	
	void setFrequencyOffset(float frequency_offset);

private:

	void modulateSamples(Symbol symbol);

	/* Configuration */
	Config conf;
	float sample_ns;  // Sample duration in ns
	float nco_1Hz;
	float center_frequency;
	unsigned int mod_rate;
	unsigned int trailer_length; // [symbols]
	Timestamp filter_delay; // [ns]


	/* State */
	enum State state;
	SymbolVector symbols;
	size_t symbols_i;
	SampleVector mod_samples;
	unsigned int mod_i;

	/* liquid-dsp objects */
	nco_crcf l_nco;
	cpfskmod l_mod;
	resamp_crcf l_resamp;     // Rational resampler

};

}; // namespace suo
