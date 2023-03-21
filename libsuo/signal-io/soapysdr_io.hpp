#pragma once

#include "suo.hpp"


namespace SoapySDR {
	class Stream;
	class Device;
};

namespace suo {

/*
 * SoapySDR I/O:
 * Main loop with SoapySDR interfacing
 */
class SoapySDRIO: public Block // SignalIO
{
public:
	struct Config {
		Config();
		
		// Configuration flags
		bool rx_on;     // Enable reception
		bool tx_on;     // Enable transmission
		bool tx_cont;   // Write TX as a continuous stream
		bool use_time;  // Enable use of stream timestamps // TODO: use_hw_time

		bool tx_active;

		bool half_duplex;

		/* Minimum delay between transmissions [ns] */
		unsigned int transmission_delay;

		/* Minimum delay the  (simple ) [ns]*/
		unsigned int idle_delay;


		// Number of samples in one RX buffer
		unsigned int buffer;
		
		/* How much ahead TX signal should be generated (samples).
		* Should usually be a few times the RX buffer length. */
		unsigned tx_latency;
		
		/* Radio sample rate for both RX and TX */
		double samplerate;
		
		/* Radio centers frequency for RX (Hz) */
		double rx_centerfreq;
		
		/* Radio center frequency for TX (Hz) */
		double tx_centerfreq;
		
		/* Radio RX gain (dB) */
		float rx_gain;
		
		/* Radio TX gain (dB) */
		float tx_gain;
		
		/* Radio RX channel number */
		size_t rx_channel;

		/* Radio TX channel number */
		size_t tx_channel;
		
		/* Radio RX antenna name */
		std::string rx_antenna;
		
		/* Radio TX antenna name */
		std::string tx_antenna;
		
		/* SoapySDR device args, such as the driver to use */
		Kwargs args;
		
		/* SoapySDR receive stream args */
		Kwargs rx_args;
		
		/* SoapySDR transmit stream args */
		Kwargs tx_args;
	};

	explicit SoapySDRIO(const Config& args = Config());
	~SoapySDRIO();

	void execute();

	void lock_tx(bool locked);

	Port<const SampleVector&, Timestamp> sinkSamples;
	SourcePort<SampleGenerator, Timestamp> generateSamples;

	Port<Timestamp> sinkTicks;

private:
	Config conf;

	SoapySDR::Device *sdr;
	SoapySDR::Stream *rxstream;
	SoapySDR::Stream *txstream;

	Timestamp current_time;

	bool tx_locked = false;
	Timestamp tx_free;
};

};


