#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>


namespace suo {

/*
 * GMSK modulator
 */
class GMSKModulator : public Block
{	
public:

	enum State {
		Idle = 0,
		Waiting,
		Transmitting,
		Trailer
	};

	struct Config
	{
		Config();

		/*
		 * Output sample rate as samples per second
		 */
		float sample_rate;

		/* 
		 * Symbol rate as symbols per second
		 */
		float symbol_rate;

		/*
		 * Center frequency (Hz)
		 */
		float center_frequency;

		/* 
		 * Frequency offset from the center frequency (Hz)
		 */
		float frequency_offset;

		/*
	 	 * Gaussian filter bandwidth-time product / excess bandwidth factor.
		 * Usually 0.3 or 0.5
		 */
		float bt;

		/*
		 * Length of the start/stop ramp in symbols
		 */
		unsigned int ramp_up_duration;
		unsigned int ramp_down_duration;
	};

	explicit GMSKModulator(const Config& conf = Config());

	~GMSKModulator();
	
	void reset();

	void sourceSamples(SampleVector& samples, Timestamp now);

	void setFrequencyOffset(float frequency_offset);

	Port<SymbolVector&, Timestamp> sourceSymbols;

private:

	void modulateSamples(Symbol symbol);

	/* Configuration */
	Config conf;
	uint32_t symrate;           // integer symbol rate
	
	Timestamp sample_ns;        // Sample duration in ns
	unsigned int mod_rate;      // GMSK modulator samples per symbols rate
	unsigned int mod_max_samples;// Maximum number of samples generated from single symbol
	float nco_1Hz;
	unsigned int trailer_length; // [symbols]
	Timestamp filter_delay; // [ns]

	/* State */
	State state;
	SymbolVector symbols;     // Symbol buffer
	int symbol_flags;
	size_t symbols_i;	      // Symnol buffer index
	Timestamp start_timestamp;
	SampleVector mod_samples;
	size_t mod_i;

	/* Liquid-DSP objects */
	gmskmod l_mod;            // GMSK modulator
	resamp_crcf l_resamp;     // Rational resampler
	nco_crcf l_nco;	          // NCO for mixing up the signal

};

}; // namespace suo
