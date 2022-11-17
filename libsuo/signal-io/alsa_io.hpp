#pragma once

#include "suo.hpp"

#ifdef ENABLE_ALSA

struct snd_pcm_t;

namespace suo {

/*
 */
class ALSAIO: public Block
{

	struct ALSAIOConf {
		// Configuration flags
		unsigned char
		rx_on:1,     // Enable reception
		tx_on:1;     // Enable transmission
		// Audio sample rate for both RX and TX
		uint32_t samplerate;
		// Number of samples in each RX block processed
		unsigned buffer;
		/* How much ahead TX signal should be generated (samples).
		* Should usually be a few times the RX buffer length. */
		unsigned tx_latency;
		// RX sound card name
		const char *rx_name;
		// TX sound card name
		const char *tx_name;
	};

	ALSAIO();
	~ALSAIO();

	void execute();
	
	Port<const SampleVector&, Timestamp> sinkSamples;
	Port<SampleVector&, Timestamp> sourceSamples;

private:
	ALSAIOConf conf;

	snd_pcm_t* rx_pcm;
	snd_pcm_t* tx_pcm;

};

}; // namespace suo

#endif
