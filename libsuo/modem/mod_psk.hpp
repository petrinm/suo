#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>

namespace suo {

/*
 * PSK demodulator
 */
class PSKModulator : public Block
{

	enum State
	{
		Idle,
		Waiting,
		Transmitting
	};

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
	
	void reset();

	void sourceSamples(SampleVector& samples, Timestamp now);

	void setFrequencyOffset(float frequency_offset);

private:
	
	void modulateSamples(Symbol symbol);

	/* Configuration */
	Config conf;
	Timestamp mf_delay_ns;
	float sample_ns;
	float nco_1Hz;

	unsigned int mod_rate;      // GMSK modulator samples per symbols rate
	unsigned int mod_max_samples;// Maximum number of samples generated from single symbol

	/* liquid-dsp and suo objects */
	modemcf l_mod;
	nco_crcf l_nco;
	resamp_crcf l_resamp;     // Rational resampler

	/* State */
	State state;
	unsigned framepos;
	unsigned symph; // Symbol clock phase
	unsigned pskph; // DPSK phase accumulator

	/* Buffers */
	Frame frame;

};

}; // namespace suo
