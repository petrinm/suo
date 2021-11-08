/* SoapySDR I/O:
 * Main loop with SoapySDR interfacing
 */

#include "suo.h"
#include "suo_macros.h"
#include "soapysdr_io.h"

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <SoapySDR/Version.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Errors.h>

#ifdef _WIN32
#include <windows.h>
#endif

struct soapysdr_io {
	struct soapysdr_io_conf conf;
	CALLBACK(sample_sink_t, sample_sink);
	CALLBACK(sample_source_t, sample_source);
};


static void soapy_fail(const char *function, int ret)
{
	fprintf(stderr, "%s failed (%d): %s\n", function, ret, SoapySDRDevice_lastError());
}
#define SOAPYCHECK(function,...) do { int ret = function(__VA_ARGS__); if(ret != 0) { soapy_fail(#function, ret); goto exit_soapy; } } while(0)



static volatile int running = 1;

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


static int soapysdr_io_execute(void *arg)
{
	struct soapysdr_io *self = arg;
	SoapySDRDevice *sdr = NULL;
	SoapySDRStream *rxstream = NULL, *txstream = NULL;
	assert(self->sample_sink && self->sample_sink_arg);
	assert(self->sample_source && self->sample_source_arg);

	const struct soapysdr_io_conf *const conf = &self->conf;

	const double sample_ns = 1.0e9 / conf->samplerate;
	const long long tx_latency_time = sample_ns * conf->tx_latency;
	const size_t rx_buflen = conf->buffer;
	// Reserve a bit more space in TX buffer to allow for timing variations
	const size_t tx_buflen = rx_buflen * 3 / 2;
	// Timeout a few times the buffer length
	const long timeout_us = sample_ns * 0.001 * 10.0 * rx_buflen;
	// Used for lost sample detection
	const long long timediff_max = sample_ns * 0.5;

	if (conf->rx_on && (self->sample_sink == NULL || self->sample_sink_arg == NULL))
		return suo_error(-999, "RX is enabled but no sample sink provided");
	if (conf->tx_on && (self->sample_source == NULL || self->sample_source_arg == NULL))
		return suo_error(-999, "TX is enabled but no sample source provided");

	if (conf->rx_on == 0 && conf->tx_on == 0)
		return suo_error(-999, "Neither RX or TX enabled");

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

	sdr = SoapySDRDevice_make(&conf->args);
	if(sdr == NULL) {
		soapy_fail("SoapySDRDevice_make", 0);
		goto exit_soapy;
	}

	if (conf->rx_on) {
		fprintf(stderr, "Configuring RX\n");
		/* On some devices (e.g. xtrx), sample rate needs to be set before
		* center frequency or the driver crashes */
		SOAPYCHECK(SoapySDRDevice_setSampleRate,
			sdr, SOAPY_SDR_RX, conf->rx_channel,
			conf->samplerate);

		SOAPYCHECK(SoapySDRDevice_setFrequency,
			sdr, SOAPY_SDR_RX, conf->rx_channel,
			conf->rx_centerfreq, NULL);

		if(conf->rx_antenna != NULL)
			SOAPYCHECK(SoapySDRDevice_setAntenna,
				sdr, SOAPY_SDR_RX, conf->rx_channel,
				conf->rx_antenna);

		SOAPYCHECK(SoapySDRDevice_setGain,
			sdr, SOAPY_SDR_RX, conf->rx_channel,
			conf->rx_gain);
	}

	if (conf->tx_on) {
		fprintf(stderr, "Configuring TX\n");
		SOAPYCHECK(SoapySDRDevice_setFrequency,
			sdr, SOAPY_SDR_TX, conf->tx_channel,
			conf->tx_centerfreq, NULL);

		if(conf->tx_antenna != NULL)
			SOAPYCHECK(SoapySDRDevice_setAntenna,
				sdr, SOAPY_SDR_TX, conf->tx_channel,
				conf->tx_antenna);

		SOAPYCHECK(SoapySDRDevice_setGain,
			sdr, SOAPY_SDR_TX, conf->tx_channel,
			conf->tx_gain);

		SOAPYCHECK(SoapySDRDevice_setSampleRate,
			sdr, SOAPY_SDR_TX, conf->tx_channel,
			conf->samplerate);
	}

#if SOAPY_SDR_API_VERSION < 0x00080000
	if (conf->rx_on) {
		SOAPYCHECK(SoapySDRDevice_setupStream,
			sdr, &rxstream, SOAPY_SDR_RX,
			SOAPY_SDR_CF32, &conf->rx_channel, 1, &conf->rx_args);
	}

	if (conf->tx_on) {
		SOAPYCHECK(SoapySDRDevice_setupStream,
			sdr, &txstream, SOAPY_SDR_TX,
			SOAPY_SDR_CF32, &conf->tx_channel, 1, &conf->tx_args);
	}
#else
	if (conf->rx_on) {
		rxstream = SoapySDRDevice_setupStream(sdr,
			SOAPY_SDR_RX, SOAPY_SDR_CF32, &conf->rx_channel, 1, &conf->rx_args);
		if(rxstream == NULL) {
			soapy_fail("SoapySDRDevice_setupStream", 0);
			goto exit_soapy;
		}
	}

	if (conf->tx_on) {
		txstream = SoapySDRDevice_setupStream(sdr,
			SOAPY_SDR_TX, SOAPY_SDR_CF32, &conf->tx_channel, 1, &conf->tx_args);
		if(txstream == NULL) {
			soapy_fail("SoapySDRDevice_setupStream", 0);
			goto exit_soapy;
		}
	}
#endif

	fprintf(stderr, "Starting streams\n");
	if (conf->rx_on)
		SOAPYCHECK(SoapySDRDevice_activateStream, sdr, rxstream, 0, 0, 0);
	if (conf->tx_on)
		SOAPYCHECK(SoapySDRDevice_activateStream, sdr, txstream, 0, 0, 0);


	/*----------------------------
	 --------- Main loop ---------
	 -----------------------------*/

	bool tx_burst_going = 0;

	long long current_time = 0;
	SoapySDRDevice_setHardwareTime(sdr, 0, "");
	if (conf->use_time)
		current_time = SoapySDRDevice_getHardwareTime(sdr, "");

	/* tx_last_end_time is when the previous produced TX buffer
	 * ended, i.e. where the next buffer should begin */
	long long tx_last_end_time = current_time + tx_latency_time;

	while(running) {
		if (conf->rx_on) {
			sample_t rxbuf[rx_buflen];
			void *rxbuffs[] = { rxbuf };
			long long rx_timestamp = 0;
			int flags = 0, ret;
			ret = SoapySDRDevice_readStream(sdr, rxstream,
				rxbuffs, rx_buflen, &flags, &rx_timestamp, timeout_us);
			if (ret > 0) {
				/* Estimate current time from the end of the received buffer.
				* If there's no timestamp, make one up by incrementing time.
				*
				* If there were no lost samples, the received buffer should
				* begin from the previous "current" time. Calculate the
				* difference to detect lost samples.
				* TODO: if configured, feed zero padding samples to receiver
				* module to correct timing after lost samples. */
				if (conf->use_time && (flags & SOAPY_SDR_HAS_TIME)) {
					long long prev_time = current_time;
					current_time = rx_timestamp + sample_ns * ret;

					long long timediff = rx_timestamp - prev_time;
					// this can produce a lot of print, not the best way to do it
					if (timediff < -timediff_max)
						fprintf(stderr, "%20lld: Time went backwards %lld ns!\n", rx_timestamp, -timediff);
					else if (timediff > timediff_max)
						fprintf(stderr, "%20lld: Lost samples for %lld ns!\n", rx_timestamp, timediff);
				} else {
					rx_timestamp = current_time; // from previous iteration
					current_time += sample_ns * ret + 0.5;
				}

				// TODO: For each sample sink
				self->sample_sink(self->sample_sink_arg, rxbuf, ret, rx_timestamp);

			} else if(ret <= 0) {
				soapy_fail("SoapySDRDevice_readStream", ret);
			}
		} else {
			/* TX-only case */
			if (conf->use_time) {
				/* There should be a blocking call somewhere, so maybe
				 * there should be some kind of a sleep here.
				 * When use_time == 0, however, the TX buffer is written
				 * until it's full and writeStream blocks, so the buffer
				 * "back-pressure" works for timing in that case.
				 * For now, in TX-only use, it's recommended to set
				 * use_time=0 and tx_cont=1. */
				current_time = SoapySDRDevice_getHardwareTime(sdr, "");
			} else {
				current_time += sample_ns * conf->buffer + 0.5;
			}
		}

#if 0
		if (conf->tx_on) {
			sample_t txbuf[tx_buflen];
			int flags = 0, ret;

			timestamp_t tx_from_time, tx_until_time;
			tx_from_time = tx_last_end_time;
			tx_until_time = current_time + tx_latency_time;
			int nsamp = round((double)(tx_until_time - tx_from_time) / sample_ns);
			//fprintf(stderr, "TX nsamp: %d\n", nsamp);

			if (nsamp > 0) {
				if ((unsigned)nsamp > tx_buflen)
					nsamp = tx_buflen;
				int ret = self->sample_source(self->sample_source_arg, txbuf, nsamp, tx_from_time);
				//assert(ntx.len >= 0 && ntx.len <= nsamp);
				//assert(ntx.end >= 0 && ntx.end <= /*ntx.len*/nsamp);
				//assert(ntx.begin >= 0 && ntx.begin <= /*ntx.len*/nsamp);

				if (conf->tx_cont) {
					// Disregard begin and end in case continuous transmit stream is configured
					//ntx.begin = 0;
					//ntx.end = ntx.len;
				}
				tx_last_end_time = tx_from_time + (timestamp_t)(sample_ns * ntx.len);
			}

			if (conf->use_time)
				flags = SOAPY_SDR_HAS_TIME;

			if (tx_burst_going && ntx.begin > 0) {
				/* If end of burst flag wasn't sent in last round,
				 * send it now together with one dummy sample.
				 * One sample is sent because trying to send
				 * zero samples gave a timeout error. */
				txbuf[0] = 0;
				const void *txbuffs[] = { txbuf };
				flags |= SOAPY_SDR_END_BURST;
				ret = SoapySDRDevice_writeStream(sdr, txstream,
					txbuffs, 1, &flags,
					tx_from_time, timeout_us);
				if(ret <= 0)
					soapy_fail("SoapySDRDevice_writeStream (end of burst)", ret);
				tx_burst_going = 0;
			}

			if (ntx.end > ntx.begin) {
				// If ntx.end does not point to end of the buffer, a burst has ended
				if (ntx.end < ntx.len) {
					flags |= SOAPY_SDR_END_BURST;
					tx_burst_going = 0;
				} else {
					tx_burst_going = 1;
				}

				const void *txbuffs[] = { txbuf + ntx.begin };

				ret = SoapySDRDevice_writeStream(sdr, txstream,
					txbuffs, ntx.end - ntx.begin, &flags,
					tx_from_time + (timestamp_t)(sample_ns * ntx.begin),
					timeout_us);
				if(ret <= 0)
					soapy_fail("SoapySDRDevice_writeStream", ret);
			} else {
				tx_burst_going = 0;
			}
		}
#else
		if (conf->tx_on) {
			sample_t txbuf[tx_buflen];
			int flags = 0, ret;

			timestamp_t tx_from_time, tx_until_time;
			tx_from_time = tx_last_end_time;
			tx_until_time = current_time + tx_latency_time;
			int nsamp = round((double)(tx_until_time - tx_from_time) / sample_ns);
			//fprintf(stderr, "TX nsamp: %d\n", nsamp);
			int tx_len = 0;

			if (nsamp > 0) {
				if ((unsigned)nsamp > tx_buflen)
					nsamp = tx_buflen;
				tx_len = self->sample_source(self->sample_source_arg, txbuf, nsamp, tx_from_time);
				tx_last_end_time = tx_from_time + (timestamp_t)(sample_ns * tx_len);
			}

			if (conf->use_time)
				flags = SOAPY_SDR_HAS_TIME;

			if (tx_burst_going && ret == 0) {
				/* If end of burst flag wasn't sent in last round,
				 * send it now together with one dummy sample.
				 * One sample is sent because trying to send
				 * zero samples gave a timeout error. */
				txbuf[0] = 0;
				const void *txbuffs[] = { txbuf };
				flags |= SOAPY_SDR_END_BURST;
				ret = SoapySDRDevice_writeStream(sdr, txstream,
					txbuffs, 1, &flags,
					tx_from_time, timeout_us);
				if(ret <= 0)
					soapy_fail("SoapySDRDevice_writeStream (end of burst)", ret);
				tx_burst_going = 0;
			}

			if (tx_len > 0) {
				//fprintf(stderr, "TX nsamp: %d\n", tx_len);
				// If ntx.end does not point to end of the buffer, a burst has ended
				if ( 1 /* ntx.end < ntx.len */) {
					flags |= SOAPY_SDR_END_BURST;
					tx_burst_going = 0;
				} else {
					tx_burst_going = 1;
				}

				const void *txbuffs[] = { txbuf };
				ret = SoapySDRDevice_writeStream(sdr, txstream,
					txbuffs, tx_len, &flags,
					tx_from_time + (timestamp_t)(sample_ns * 0),
					timeout_us);
				if(ret <= 0)
					soapy_fail("SoapySDRDevice_writeStream", ret);
			} else {
				tx_burst_going = 0;
			}
		}
#endif
	}

	fprintf(stderr, "Stopped receiving\n");

exit_soapy:
	//deinitialize(suo); //TODO moved somewhere else

	if(rxstream != NULL) {
		fprintf(stderr, "Deactivating stream\n");
		SoapySDRDevice_deactivateStream(sdr, rxstream, 0, 0);
		SoapySDRDevice_closeStream(sdr, rxstream);
	}
	if(sdr != NULL) {
		fprintf(stderr, "Closing device\n");
		SoapySDRDevice_unmake(sdr);
	}

	fprintf(stderr, "Done\n");
	return 0;
}


static void *soapysdr_io_init(const void *conf)
{
	struct soapysdr_io *self = calloc(1, sizeof(struct soapysdr_io));
	if (self == NULL) return NULL;
	self->conf = *(struct soapysdr_io_conf*)conf;
	if (strcmp(SoapySDR_getABIVersion(), SOAPY_SDR_ABI_VERSION) != 0)
		fprintf(stderr, "Warning: Wrong SoapySDR ABI version\n");
	return self;
}


static int soapysdr_io_destroy(void *arg)
{
	struct soapysdr_io *self = arg;
	self->sample_sink = NULL;
	self->sample_sink_arg = NULL;
	self->sample_source = NULL;
	self->sample_source_arg = NULL;
	free(self);
	return 0;
}


static int soapysdr_io_set_sample_sink(void *arg, sample_sink_t callback, void *callback_arg)
{
	struct soapysdr_io *self = arg;
	if (callback == NULL || callback_arg == NULL)
		return suo_error(-3, "NULL sample sink");
	self->sample_sink = callback;
	self->sample_sink_arg = callback_arg;
	return 0;
}


static int soapysdr_io_set_sample_source(void *arg, sample_source_t callback, void *callback_arg)
{
	struct soapysdr_io *self = arg;
	if (callback == NULL || callback_arg == NULL)
		return suo_error(-3, "NULL sample source");
	self->sample_source = callback;
	self->sample_source_arg = callback_arg;
	return 0;
}



const struct soapysdr_io_conf soapysdr_io_defaults = {
	.buffer = 2048,
	.rx_on = 1,
	.tx_on = 1,
	.tx_cont = 0,
	.use_time = 1,
	.tx_latency = 8192,
	.samplerate = 1e6,
	.rx_centerfreq = 433.8e6,
	.tx_centerfreq = 433.8e6,
	.rx_gain = 60,
	.tx_gain = 80,
	.rx_channel = 0,
	.tx_channel = 0,
	.rx_antenna = NULL,
	.tx_antenna = NULL
};

CONFIG_BEGIN(soapysdr_io)
CONFIG_I(rx_on)
CONFIG_I(tx_on)
CONFIG_I(tx_cont)
CONFIG_I(use_time)
CONFIG_I(buffer)
CONFIG_I(tx_latency)
CONFIG_F(samplerate)
CONFIG_F(rx_centerfreq)
CONFIG_F(tx_centerfreq)
CONFIG_F(rx_gain)
CONFIG_F(tx_gain)
CONFIG_I(rx_channel)
CONFIG_I(tx_channel)
CONFIG_C(rx_antenna)
CONFIG_C(tx_antenna)
	if (strncmp(parameter, "soapy-", 6) == 0) {
		SoapySDRKwargs_set(&c->args, parameter+6, value);
		return 0;
	}
	if (strncmp(parameter, "device:", 7) == 0) {
		SoapySDRKwargs_set(&c->args, parameter+7, value);
		return 0;
	}
	if (strncmp(parameter, "rx_stream:", 10) == 0) {
		SoapySDRKwargs_set(&c->rx_args, parameter+10, value);
		return 0;
	}
	if (strncmp(parameter, "tx_stream:", 10) == 0) {
		SoapySDRKwargs_set(&c->tx_args, parameter+10, value);
		return 0;
	}
CONFIG_END()


const struct signal_io_code soapysdr_io_code = {
	.name="soapysdr_io",
	.init=soapysdr_io_init,
	.destroy=soapysdr_io_destroy,
	.init_conf=init_conf,
	.set_conf=set_conf,
	.set_sample_sink=soapysdr_io_set_sample_sink,
	.set_sample_source=soapysdr_io_set_sample_source,
	.execute=soapysdr_io_execute
};
