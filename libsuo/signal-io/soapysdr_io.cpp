
#include "suo.hpp"
#include "registry.hpp"
#include "signal-io/soapysdr_io.hpp"

#include <string>
#include <iostream>
#include <signal.h>
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


static int running = 1;

#ifdef _WIN32
static BOOL WINAPI winhandler(DWORD ctrl)
{
	switch (ctrl) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		running = 0;
		return TRUE;
	default:
		return FALSE;
	}
}
#else
static void sighandler(int sig)
{
	(void)sig;
	running = 0;
}
#endif


SoapySDRIO::Config::Config() {
	buffer = 2048;
	rx_on = 1;
	tx_on = 1;
	tx_cont = 0;
	use_time = 1;
	tx_latency = 8192;
	samplerate = 1e6;
	rx_centerfreq = 433.8e6;
	tx_centerfreq = 433.8e6;
	rx_gain = 60;
	tx_gain = 80;
	rx_channel = 0;
	tx_channel = 0;
	rx_antenna = "";
	tx_antenna = "";
}


SoapySDRIO::SoapySDRIO(const Config& conf) :
	conf(conf)
{
}

SoapySDRIO::~SoapySDRIO() {

}


void SoapySDRIO::execute()
{

	const double sample_ns = 1.0e9 / conf.samplerate;
	const long long tx_latency_time = sample_ns * conf.tx_latency;
	const size_t rx_buflen = conf.buffer;
	// Reserve a bit more space in TX buffer to allow for timing variations
	const size_t tx_buflen = rx_buflen * 3 / 2;
	// Timeout a few times the buffer length
	const long timeout_us = sample_ns * 0.001 * 10.0 * rx_buflen;
	// Used for lost sample detection
	const long long timediff_max = sample_ns * 0.5;

	//if (conf.rx_on && (sample_sink == NULL || sample_sink_arg == NULL))
	///	throw SuoError("RX is enabled but no sample sink provided");
	if (conf.tx_on)
		throw SuoError("TX is enabled but no sample source provided");

	if (conf.rx_on == 0 && conf.tx_on == 0)
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

	sdr = SoapySDR::Device::make(conf.args);
	if(sdr == NULL) {
		throw SuoError("sdr->make");
	}

#if 0
	cerr << "Configuring USRP GPIO" << endl;
	unsigned int gpio_mask = 0x100;
	sdr->writeGPIO("FP0:CTRL", gpio_mask, gpio_mask);
	sdr->writeGPIO("FP0:DDR", gpio_mask, gpio_mask);
	sdr->writeGPIO("FP0:ATR_0X", 0, gpio_mask);
	sdr->writeGPIO("FP0:ATR_RX", 0, gpio_mask);
	sdr->writeGPIO("FP0:ATR_TX", gpio_mask, gpio_mask);
	sdr->writeGPIO("FP0:ATR_XX", gpio_mask, gpio_mask);
#endif

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

#if SOAPY_SDR_API_VERSION < 0x00080000
	if (conf.rx_on) {
		sdr->setupStream(&rxstream, SOAPY_SDR_RX,
			SOAPY_SDR_CF32, &conf.rx_channel, 1, &conf.rx_args);
	}

	if (conf.tx_on) {
		sdr->setupStream(&txstream, SOAPY_SDR_TX,
			SOAPY_SDR_CF32, &conf.tx_channel, 1, &conf.tx_args);
	}
#else
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
#endif

	cerr << "Starting streams" << endl;
	if (conf.rx_on)
		sdr->activateStream(rxstream);
	if (conf.tx_on)
		sdr->activateStream(txstream);


	/*----------------------------
	 --------- Main loop ---------
	 -----------------------------*/

	bool tx_burst_going = 0;
	Timestamp current_time = 0;

	if (conf.use_time) {
		sdr->setHardwareTime(0); // Reset timer
		usleep(20 * 1000);
		current_time = sdr->getHardwareTime();
	}

	/* tx_last_end_time is when the previous produced TX buffer
	 * ended, i.e. where the next buffer should begin */
	Timestamp tx_last_end_time = (Timestamp)current_time + tx_latency_time;

	SampleVector rxbuf;
	rxbuf.reserve(rx_buflen);

	SampleVector txbuf;
	txbuf.reserve(tx_buflen);

	void* rxbuffs[] = { rxbuf.data() };
	const void* txbuffs[] = { txbuf.data() };


	while(running) {
		if (conf.rx_on) {
	
			long long rx_timestamp = 0;
			int flags = 0, ret;
			ret = sdr->readStream(rxstream, rxbuffs, rxbuf.capacity(), flags, rx_timestamp, timeout_us);
			if (ret > 0) {
				rxbuf.resize(ret);

				/* Estimate current time from the end of the received buffer.
				* If there's no timestamp, make one up by incrementing time.
				*
				* If there were no lost samples, the received buffer should
				* begin from the previous "current" time. Calculate the
				* difference to detect lost samples.
				* TODO: if configured, feed zero padding samples to receiver
				* module to correct timing after lost samples. */
				if (conf.use_time && (flags & SOAPY_SDR_HAS_TIME)) {
					long long prev_time = current_time;
					current_time = rx_timestamp + sample_ns * ret;

					long long timediff = rx_timestamp - prev_time;
					// this can produce a lot of print, not the best way to do it
					if (timediff < -timediff_max)
						cerr << rx_timestamp << ": Time went backwards " << -timediff  << " ns!" << endl;
					else if (timediff > timediff_max)
						cerr << rx_timestamp << ": Lost samples for " << timediff << "  ns!" << endl;
					
				} else {
					rx_timestamp = current_time; // from previous iteration
					current_time += sample_ns * ret + 0.5;
				}

				if (tx_burst_going == 0 /*&& !c.half_duplex*/)
					sinkSamples.emit(rxbuf, rx_timestamp);

			}
			else if (ret == SOAPY_SDR_OVERFLOW) {
				cerr << "RX OVERFLOW" << endl;
			} else if(ret <= 0) {
				throw SuoError("sdr->readStream: %d", ret);
			}
		} else {
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
			int flags = 0, ret = 0;

			Timestamp tx_from_time = tx_last_end_time;
			Timestamp tx_until_time = (Timestamp)current_time + tx_latency_time;
			int nsamp = round((double)(tx_until_time - tx_from_time) / sample_ns);

			//fprintf(stderr, "TX nsamp: %d\n", nsamp);
			int tx_len = 0;

			if (nsamp > 0) {
				if ((unsigned)nsamp > tx_buflen)
					nsamp = tx_buflen;
				sourceSamples.emit(txbuf, tx_from_time);
				tx_last_end_time = tx_from_time + (Timestamp)(sample_ns * tx_len);
			}

			//if (conf.use_time)
			//	flags = SOAPY_SDR_HAS_TIME;

			if (tx_burst_going && tx_len == 0) {
				/* If end of burst flag wasn't sent in last round,
				 * send it now together with one dummy sample.
				 * One sample is sent because trying to send
				 * zero samples gave a timeout error. */
				txbuf[0] = 0;

				flags |= SOAPY_SDR_END_BURST;
				ret = sdr->writeStream(txstream, txbuffs, 1, flags, tx_from_time, timeout_us);
				if(ret <= 0)
					throw SuoError("sdr->writeStream (end of burst)", ret);
				tx_burst_going = 0;
			}

			if (tx_len > 0) {
				//fprintf(stderr, "TX nsamp: %d\n", tx_len);
				// If ntx.end does not point to end of the buffer, a burst has ended
				if ( 0 /* tx_len < nsamp */) {
					flags |= SOAPY_SDR_END_BURST;
					tx_burst_going = 0;
				} else {
					tx_burst_going = 1;
				}

				Timestamp t = tx_from_time + (Timestamp)(sample_ns * 0);
				ret = sdr->writeStream(txstream, txbuffs, txbuf.size(), flags, t, timeout_us);
				if(ret <= 0)
					throw SuoError("sdr->writeStream %d", ret);
			} else {
				tx_burst_going = 0;
			}
		}

		/* Send ticks */
		if (1 /* sinkTicks.empty() */) {

			unsigned int flags = 0;

			if (1) // TODO
				flags |= SUO_FLAGS_RX_ACTIVE;
			if (0) // TODO
				flags |= SUO_FLAGS_RX_LOCKED;
			if (tx_burst_going == 1 || tx_last_end_time > (Timestamp)current_time)
				flags |= SUO_FLAGS_TX_ACTIVE;

			sinkTicks.emit(flags, current_time);
		}

	}

	cerr << "Stopped receiving" << endl;

exit_soapy:
	//deinitialize(suo); //TODO moved somewhere else

	if(rxstream != NULL) {
		fprintf(stderr, "Deactivating stream\n");
		sdr->deactivateStream(rxstream, 0, 0);
		sdr->closeStream(rxstream);
	}
	if(sdr != NULL) {
		fprintf(stderr, "Closing device\n");
		sdr->unmake(sdr);
	}

	fprintf(stderr, "Done\n");
}


Block* createSoapySDRIO(const Kwargs& args)
{
	return new SoapySDRIO();
}

static Registry registerSoapySDRIO("SoapySDRIO", &createSoapySDRIO);
