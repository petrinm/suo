/* Transmitter for phase-shift keying.
 * Currently developed to transmit TETRA PI/4 DQPSK. */
#include "mod_psk.h"
#include "suo_macros.h"
#include "ddc.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <liquid/liquid.h>

#define FRAMELEN_MAX 0x900
#define OVERSAMP 4

enum frame_state { FRAME_NONE, FRAME_WAIT, FRAME_TX };

struct mod_psk {
	/* Configuration */
	struct mod_psk_conf c;
	timestamp_t mf_delay_ns;
	float sample_ns;

	/* Callbacks */
	const struct tx_input_code *input;
	void *input_arg;

	/* liquid-dsp and suo objects */
	struct suo_ddc *duc;
	firfilt_crcf l_mf; // Matched filter

	/* State */
	enum frame_state state;
	unsigned framepos;
	unsigned symph; // Symbol clock phase
	unsigned pskph; // DPSK phase accumulator

	/* Buffers */
	struct frame frame;
	/* Allocate space for flexible array member */
	bit_t frame_buffer[FRAMELEN_MAX];
};



static void* mod_psk_init(const void *conf_v)
{
	struct mod_psk* self = calloc(1, sizeof(*self));
	if(self == NULL) return NULL;
	self->c = *(const struct mod_psk_conf*)conf_v;

	// sample rate for the modulator
	float fs_mod = self->c.symbolrate * OVERSAMP;
	self->sample_ns = 1.0e9f / fs_mod;
	self->duc = suo_ddc_init(fs_mod, self->c.samplerate, self->c.centerfreq, 1);

	// design the matched filter
#define MFDELAY (3)
#define MFTAPS (MFDELAY*OVERSAMP*2+1)
	float taps[MFTAPS];
	liquid_firdes_rrcos(OVERSAMP, MFDELAY, 0.35, 0, taps);
	self->l_mf = firfilt_crcf_create(taps, MFTAPS);
	self->mf_delay_ns = self->sample_ns * (MFDELAY*OVERSAMP);


	//self->l_mod = modem_create(LIQUID_MODEM_PSK2);

	// For initial testing:

	return self;
}

static int mod_psk_destroy(void *arg)
{
	(void)arg;
	return 0;
}

static void get_next_frame(struct mod_psk *self, timestamp_t timenow, timestamp_t time_end)
{
	if (self->state == FRAME_NONE) {
		int fl = self->input->get_frame(self->input_arg, &self->frame, FRAMELEN_MAX, time_end);
		if (fl >= 0) {
			struct frame_header* hdr = &self->frame.hdr;
			if (hdr->timestamp != 0) { // valid time?
				self->state = FRAME_WAIT;
				int64_t timediff = timenow - hdr->timestamp;
				if (timediff > 0) {
					fprintf(stderr, "Warning: TX frame late by %ld ns\n", timediff);
					if (hdr->flags & SUO_FLAGS_NO_LATE)
						self->state = FRAME_NONE;
				}
			} else {
				self->state = FRAME_TX;
			}
		}
	}
}


static tx_return_t mod_psk_execute(void *arg, sample_t *samples, size_t maxsamples, timestamp_t timestamp)
{
	const float pi_4f = 0.7853981633974483f;
	struct mod_psk *self = arg;

	timestamp += self->mf_delay_ns;
	size_t buflen = suo_duc_in_size(self->duc, maxsamples, &timestamp);
	self->input->tick(self->input_arg, timestamp);

	size_t i;
	sample_t buf[buflen];

	const float sample_ns = self->sample_ns;
	const float amp = 0.5f; // Amplitude
	const timestamp_t time_end = timestamp + (timestamp_t)(sample_ns * buflen);

	unsigned symph = self->symph; // Symbol clock phase
	unsigned pskph = self->pskph; // DPSK phase accumulator
	unsigned framepos = self->framepos;

	if (self->state == FRAME_NONE)
		get_next_frame(self, timestamp, time_end);

	for (i = 0; i < buflen; i++) {
		sample_t s = 0;
		timestamp_t timenow = timestamp + (timestamp_t)(sample_ns * i);
		if (self->state == FRAME_WAIT) {
			/* Frame is waiting to be transmitted */
			int64_t timediff = timenow - self->frame.hdr.timestamp;
			if (timediff >= 0) {
				self->state = FRAME_TX;
				symph = 0;
				//fprintf(stderr, "%lu: Starting transmission\n", self->frame.timestamp);
			}
		}
		if (self->state == FRAME_TX && symph == 0) {
			unsigned bit0, bit1;
			bit0 = self->frame.data[framepos]   & 1;
			bit1 = self->frame.data[framepos+1] & 1;

			if (bit0 == 1 && bit1 == 1)
				pskph -= 3;
			else if (bit0 == 0 && bit1 == 1)
				pskph += 3;
			else if (bit0 == 0 && bit1 == 0)
				pskph += 1;
			else
				pskph -= 1;
			pskph &= 7;
			s = (cosf(pi_4f * pskph) + I*sinf(pi_4f * pskph)) * amp;

			framepos += 2;
			if (framepos+1 >= self->frame.data_len) {
				framepos = 0;
				self->state = FRAME_NONE;
				get_next_frame(self, timestamp, time_end);
			}
		}
		firfilt_crcf_push(self->l_mf, s);
		firfilt_crcf_execute(self->l_mf, &buf[i]);
		symph = (symph + 1) % OVERSAMP;
	}
	self->symph = symph;
	self->pskph = pskph;
	self->framepos = framepos;

	tx_return_t retv = suo_duc_execute(self->duc, buf, buflen, samples);
	assert((size_t)retv.len <= maxsamples);
	//fprintf(stderr, "(%d %d %d) ", retv.len, retv.begin, retv.end);

	return retv;
}



static int mod_psk_set_callbacks(void *arg, const struct tx_input_code *input, void *input_arg)
{
	struct mod_psk *self = arg;
	self->input = input;
	self->input_arg = input_arg;
	return 0;
}



const struct mod_psk_conf mod_psk_defaults = {
	.samplerate = 1e6,
	.symbolrate = 18000,
	.centerfreq = 100000
};


CONFIG_BEGIN(mod_psk)
CONFIG_F(samplerate)
CONFIG_F(symbolrate)
CONFIG_F(centerfreq)
CONFIG_END()

const struct transmitter_code mod_psk_code = {
	.name = "mod_psk",
	.init = mod_psk_init,
	.destroy = mod_psk_destroy,
	.init_conf = init_conf, // Generate by the CONFIG-macro
	.set_conf = set_conf, // Generate by the CONFIG-macro
	.set_callbacks = mod_psk_set_callbacks,
	.execute = mod_psk_execute
};
