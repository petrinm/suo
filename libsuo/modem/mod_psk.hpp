#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>

namespace suo {

/*
 * PSK demodulator
 */
class PSKModulator : public Block
{
public:

	struct Config {
		Config();

		/* */
		float sample_rate;

		/* */
		float symbol_rate;

		/* */
		float center_frequency;

		/* */
		float frequency_offset;

		/* */
		float amplitude;

		/* Length of the start/stop ramp in symbols */
		unsigned int ramp_up_duration, ramp_down_duration;

	};


	explicit PSKModulator(const Config& conf = Config());
	~PSKModulator();
	
	PSKModulator(const PSKModulator&) = delete;
	PSKModulator& operator=(const PSKModulator&) = delete;


	void reset();

	//void sourceSamples(SampleVector& samples, Timestamp now);
	SampleGenerator generateSamples(Timestamp now);

	void setFrequencyOffset(float frequency_offset);

	SourcePort<SymbolGenerator, Timestamp> generateSymbols;
	//Port<SymbolVector&, Timestamp> sourceSymbols;
	
private:
	
	SampleGenerator sampleGenerator(); // SymbolGenerator& gen);

	/* Configuration */
	Config conf;
	Timestamp filter_delay;
	float sample_ns;
	unsigned int mod_rate;
	float nco_1Hz;
	
	/* State */
	SymbolVector symbols;
	SymbolGenerator symbol_gen;

	/* liquid-dsp and suo objects */
	modemcf l_mod;
	nco_crcf l_nco;
	resamp_crcf l_resamp;     // Rational resampler
};

}; // namespace suo
