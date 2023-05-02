#pragma once

#include "suo.hpp"
#include <liquid/liquid.h>


namespace suo {


enum CostasLoopOrder {
	CostasLoopBPSK,
	CostasLoopQPSK,
	CostasLoopGMSK,
	CostasLoop8PSK,
};

/*
 * GMSK demodulator
 */
class PSKDemodulator : public Block
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
		 * Maximum allowed frequency offset (Hz)
		 */
		float maximum_frequency_offset;

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

		float agc_bandwidth0;
		float agc_bandwidth1;

		float afc_speed0;
		float afc_speed1;

		float symsync_bw0;
		float symsync_bw1;
	};

	explicit PSKDemodulator(const Config& conf = Config());
	~PSKDemodulator();

	PSKDemodulator(const PSKDemodulator&) = delete;
	PSKDemodulator& operator=(const PSKDemodulator&) = delete;

	void reset();

	void sinkSamples(const SampleVector& samples, Timestamp timestamp);

	void lockReceiver(bool locked, Timestamp now);

	void setFrequencyOffset(float frequency_offset);

	Port<Symbol, Timestamp> sinkSymbol;
	Port<SoftSymbol, Timestamp> sinkSoftSymbol;
	Port<const std::string&, const MetadataValue&> setMetadata;

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

	float freq_min, freq_max;

	/* liquid-dsp objects */
	nco_crcf l_nco;
	modemcf l_mod;
	resamp_crcf l_resamp;
	agc_crcf l_agc;
	symsync_crcf l_sync;
	windowcf l_demod_delay;

	/* Buffers */
	Frame* frame;

};

}; // namespace suo

