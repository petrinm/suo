
#include "suo.hpp"
#include "registry.hpp"
#include "signal-io/soapysdr_io.hpp"

#include <string>
#include <iostream>
#include <signal.h>
#include <unistd.h> // usleep
#include <assert.h>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Errors.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace suo;
using namespace std;


static bool running = true;

#ifdef _WIN32
static BOOL WINAPI winhandler(DWORD ctrl)
{
	switch (ctrl) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		running = false;
		return TRUE;
	default:
		return FALSE;
	}
}
#else
static void sighandler(int sig)
{
	(void)sig;
	running = false;
}
#endif


SoapySDRIO::Config::Config() {
	buffer = 2048;
	rx_on = true;
	tx_on = true;
	tx_cont = false;
	half_duplex = true;
	use_time = true;
	tx_latency = 8192;
	samplerate = 1e6;
	rx_centerfreq = 433e6; // [Hz]
	tx_centerfreq = 433e6; // [Hz]
	rx_gain = 60;
	tx_gain = 80;
	rx_channel = 0;
	tx_channel = 0;
	rx_antenna = "";
	tx_antenna = "";
}


SoapySDRIO::SoapySDRIO(const Config& conf) :
	conf(conf),
	sdr(NULL),
	rxstream(NULL),
	txstream(NULL)
{
}

SoapySDRIO::~SoapySDRIO() {

	if (rxstream != NULL) {
		cerr << "Deactivating RX stream" << endl;
		sdr->deactivateStream(rxstream, 0, 0);
		sdr->closeStream(rxstream);
	}

	if (txstream != NULL) {
		cerr << "Deactivating TX stream" << endl;
		sdr->deactivateStream(txstream, 0, 0);
		sdr->closeStream(txstream);
	}

	if (sdr != NULL) {
		cerr << "Closing device" << endl;
		SoapySDR::Device::unmake(sdr);
	}

}

// SoapySDR::timeNsToTicks(

void SoapySDRIO::execute()
{

	const double sample_ns = 1.0e9 / conf.samplerate;
	const long long tx_latency_time = sample_ns * conf.tx_latency;
	const size_t rx_buflen = conf.buffer;
	// Reserve a bit more space in TX buffer to allow for timing variations
	//const size_t tx_buflen = 610000 - 1000;
	const size_t tx_buflen = 4e6; //8 * rx_buflen; // (rx_buflen * 3) / 2;
	// Timeout a few times the buffer length
	const long timeout_us = (sample_ns * rx_buflen) * 0.1;
	// Used for lost sample detection
	const long long timediff_max = sample_ns * 0.5;

	//if (conf.rx_on && (sample_sink == NULL || sample_sink_arg == NULL))
	///	throw SuoError("RX is enabled but no sample sink provided");
	//if (conf.tx_on)
	//	throw SuoError("TX is enabled but no sample source provided");

	if (conf.rx_on == false && conf.tx_on == false)
		throw SuoError("Neither RX or TX enabled");

	/*--------------------------------
	 ---- Hardware initialization ----
	 ---------------------------------*/

#ifdef _WIN32
	SetConsoleCtrlHandler(winhandler, TRUE);
#else
	{
		struct sigaction sigact;
		sigact.sa_handler = sighandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = 0;
		sigaction(SIGINT, &sigact, NULL);
		sigaction(SIGTERM, &sigact, NULL);
		sigaction(SIGQUIT, &sigact, NULL);
		sigaction(SIGPIPE, &sigact, NULL);
	}
#endif

	cout << "Soapy args: ";
	for (auto pair: conf.args)
		cout << pair.first << "=" << pair.second << ", ";
	cout << endl;

	sdr = SoapySDR::Device::make(conf.args);
	if (sdr == NULL)
		throw SuoError("Failed to open SoapyDevice");

	if (conf.rx_on) {
		cerr << "Configuring RX" << endl;
		/* On some devices (e.g. xtrx), sample rate needs to be set before
		 * center frequency or the driver crashes */
		sdr->setSampleRate(SOAPY_SDR_RX, conf.rx_channel, conf.samplerate);
		sdr->setFrequency(SOAPY_SDR_RX, conf.rx_channel, conf.rx_centerfreq);

		if(conf.rx_antenna.empty() == false)
			sdr->setAntenna(SOAPY_SDR_RX, conf.rx_channel, conf.rx_antenna);

		sdr->setGain(SOAPY_SDR_RX, conf.rx_channel, conf.rx_gain);
	}

	if (conf.tx_on) {
		cerr << "Configuring TX" << endl;
		sdr->setFrequency(SOAPY_SDR_TX, conf.tx_channel, conf.tx_centerfreq);

		if(conf.tx_antenna.empty() == false)
			sdr->setAntenna(SOAPY_SDR_TX, conf.tx_channel, conf.tx_antenna);

		sdr->setGain(SOAPY_SDR_TX, conf.tx_channel, conf.tx_gain);
		sdr->setSampleRate(SOAPY_SDR_TX, conf.tx_channel, conf.samplerate);
	}

	if (conf.rx_on) {
		std::vector<size_t> rx_channels = { conf.rx_channel };
		rxstream = sdr->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32, rx_channels, conf.rx_args);
		if(rxstream == NULL)
			throw SuoError("Failed to create RX stream");
	}

	if (conf.tx_on) {
		std::vector<size_t> tx_channels = { conf.tx_channel };
		txstream = sdr->setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32, tx_channels, conf.tx_args);
		if(txstream == NULL)
			throw SuoError("Failed to create TX stream");
	}

	cerr << "Starting streams" << endl;
	if (rxstream)
		sdr->activateStream(rxstream);
	if (txstream && conf.tx_active)
		sdr->activateStream(txstream);


	/*----------------------------
	 --------- Main loop ---------
	 -----------------------------*/

	bool tx_active = false;
	current_time = 0;

	if (conf.use_time) {
		sdr->setHardwareTime(0); // Reset timer
		usleep(20 * 1000);
		current_time = sdr->getHardwareTime();
	}

	/* tx_last_end_time is when the previous produced TX buffer
	 * ended, i.e. where the next buffer should begin */
	Timestamp tx_last_end_time = (Timestamp)current_time + tx_latency_time;

	SampleVector rxbuf, txbuf;
	rxbuf.resize(rx_buflen);
	txbuf.resize(tx_buflen);

	// Array of buffers for Soapy interface
	void* rxbuffs[] = { rxbuf.data() };
	const void* txbuffs[] = { txbuf.data() };

	SampleGenerator sample_gen;

	while(running) {

		if (conf.rx_on && tx_active == false) {

			long long rx_timestamp = 0;
			int rx_flags = 0;
			int ret = sdr->readStream(rxstream, rxbuffs, rx_buflen, rx_flags, rx_timestamp, timeout_us);
			//cout << "rx_timestamp " << rx_timestamp << endl;
			if (ret > 0) {

				rxbuf.timestamp = rx_timestamp;
				const size_t new_samples = (size_t)ret;
				rxbuf.resize(new_samples);
				//if (new_samples != rx_buflen)
				//	cout << "Received only " << new_samples << " samples" << endl;
				

				/* Estimate current time from the end of the received buffer.
				 * If there's no timestamp, make one up by incrementing time.
				 *
				 * If there were no lost samples, the received buffer should
				 * begin from the previous "current" time. Calculate the
				 * difference to detect lost samples.
				 * TODO: if configured, feed zero padding samples to receiver
				 * module to correct timing after lost samples. */
				if (conf.use_time && (rx_flags & SOAPY_SDR_HAS_TIME)) {
					long long prev_time = current_time;
					current_time = rx_timestamp + sample_ns * new_samples;

					// Time jump warnings
					// This can produce a lot of print, not the best way to do it
					long long timediff = rx_timestamp - prev_time;
					if (timediff < -timediff_max)
						cerr << rx_timestamp << ": Time went backwards " << -timediff  << " ns!" << endl;
					else if (timediff > timediff_max)
						cerr << rx_timestamp << ": Lost samples for " << timediff << "  ns!" << endl;
					
				} else {
					/* No hardware timestamps supported so estimate current
					 * timestamp from previous iteration */
					rx_timestamp = current_time;
					current_time += sample_ns * new_samples + 0.5; // +0.5 to ensure rounding up
				}

				// Pass the samples to other blocks
				if (!(tx_active && conf.half_duplex))
					sinkSamples.emit(rxbuf, rx_timestamp);

			}
			else if (ret == SOAPY_SDR_OVERFLOW) {
				cerr << "RX OVERFLOW" << endl;
			} else if(ret < 0) {
				throw SuoError("sdr->readStream: %d", ret);
			}

		}
		else {
			/* TX-only case */
			if (conf.use_time) {
				/* There should be a blocking call somewhere, so maybe
				 * there should be some kind of a sleep here.
				 * When use_time == 0, however, the TX buffer is written
				 * until it's full and writeStream blocks, so the buffer
				 * "back-pressure" works for timing in that case.
				 * For now, in TX-only use, it's recommended to set
				 * use_time=0 and tx_cont=1. */
				current_time = sdr->getHardwareTime();
			} else {
				current_time += sample_ns * conf.buffer + 0.5;
			}
		}

		if (conf.tx_on) {


			Timestamp tx_from_time = tx_last_end_time;
			//Timestamp tx_until_time = (Timestamp)current_time + tx_latency_time;
			//size_t new_sample = round((double)(tx_until_time - tx_from_time) / sample_ns);
			//new_sample = max(new_sample, tx_buflen);


			if (tx_active) {
				/*
				 * Transmission/Burst on going on
				 */

				// Generate new samples 
				int tx_flags = 0;
				txbuf.clear();
				sample_gen.sourceSamples(txbuf);

				if (txbuf.empty()) {
					/* If end of burst flag wasn't sent in last round,
					 * send it now together with one dummy sample.
					 * One sample is sent because trying to send
					 * zero samples gave a timeout error. */

					txbuf.resize(1);
					txbuf[0] = 0;

					tx_flags |= SOAPY_SDR_END_BURST;

					cout << "Warning: End of TX samples without end_of_burst!" << endl;
				}

				if (sample_gen.running() == false || (txbuf.flags & VectorFlags::end_of_burst) != 0)
					tx_flags |= SOAPY_SDR_END_BURST;

				/* Write what we have */
				Timestamp t = tx_from_time + (Timestamp)(sample_ns * 0);
				int flags = tx_flags;
				int ret = sdr->writeStream(txstream, txbuffs, txbuf.size(), flags, t);
				cout << " sdr->writeStream " << ret << endl;
				if (ret <= 0)
					throw SuoError("sdr->writeStream %d", ret);

				// Deactivate txstream
				if ((tx_flags & SOAPY_SDR_END_BURST) != 0) {
					cout << "end of burst" << endl;
					if (conf.tx_active == false)
						sdr->deactivateStream(txstream);
					tx_active = false; 
				}
				
			}
			else {

				//tx_last_end_time = tx_from_time + (Timestamp)(sample_ns * txbuf.size());

				sample_gen = generateSamples.emit(tx_from_time);
				if (sample_gen.running()) {

					// Source samples
					sample_gen.sourceSamples(txbuf);
					cout << "start of burst " << txbuf.size() << endl;

					/* New burst or start of transmission */
					int tx_flags = 0;
					tx_active = true;

					if ((txbuf.flags & VectorFlags::start_of_burst) == 0)
						cout << "Warning: start of burst no properly marked!" << endl;

					if (txbuf.flags & VectorFlags::has_timestamp)
						tx_flags |= SOAPY_SDR_HAS_TIME;

					if ((txbuf.flags & VectorFlags::end_of_burst) != 0) {
						tx_flags |= SOAPY_SDR_END_BURST;
						tx_active = false;
					}

					Timestamp t = tx_from_time + (Timestamp)(sample_ns * 0);
					
					if (conf.tx_active == false)
						sdr->activateStream(txstream, tx_flags, t);

					// Write samples to buffer
					int ret = sdr->writeStream(txstream, txbuffs, txbuf.size(), tx_flags, t, timeout_us);
					cout << " sdr->writeStream " << ret << endl;
					if ((size_t)ret != txbuf.size())
						throw SuoError("sdr->writeStream %d", ret);
						
				}

			}

		}

		/* Send ticks */
		if (1) {
			sinkTicks.emit(current_time);
		}

	}

	cerr << "Stopped receiving" << endl;

}


Block* createSoapySDRIO(const Kwargs& args)
{
	return new SoapySDRIO();
}

static Registry registerSoapySDRIO("SoapySDRIO", &createSoapySDRIO);
