#include "mod_fsk.h"
#include "suo_macros.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <liquid/liquid.h>

#define FRAMELEN_MAX 0x900
static const float pi2f = 6.283185307179586f;

struct mod_fsk {
	/* Configuration */
	struct mod_fsk_conf c;
	uint32_t symrate; // integer symbol rate
	float sample_ns; // Sample duration in ns
	float freq0, freq1;

	/* State */
	char transmitting; // 0 = no frame, 1 = frame waiting, 2 = transmitting
	unsigned framelen, framepos;
	uint32_t symphase;

	/* liquid-dsp objects */
	nco_crcf l_nco;

	/* Callbacks */
	CALLBACK(symbol_source_t, symbol_source);

	/* Buffers */
	struct frame frame;
	/* Allocate space for flexible array member */
	bit_t frame_buffer[FRAMELEN_MAX];
};


static void* mod_fsk_init(const void *conf_v)
{
	struct mod_fsk *self = (struct mod_fsk *)malloc(sizeof(struct mod_fsk));
	if(self == NULL) return NULL;
	memset(self, 0, sizeof(struct mod_fsk));

	self->c = *(const struct mod_fsk_conf*)conf_v;
	const float samplerate = self->c.samplerate;

	self->symrate = 4294967296.0f * self->c.symbolrate / samplerate;
	self->sample_ns = 1.0e9f / samplerate;

	/*
	 * Init NCO for FSK generating
	 */
 	const float deviation = pi2f * self->c.modindex * 0.5f * self->c.symbolrate / samplerate;
 	const float cf = pi2f * self->c.centerfreq / samplerate;
 	self->freq0 = cf - deviation;
 	self->freq1 = cf + deviation;
	self->l_nco = nco_crcf_create(LIQUID_NCO);

	return self;
}


static int mod_fsk_destroy(void *arg)
{
	struct mod_fsk *self = (struct mod_fsk *)arg;
	nco_crcf_destroy(self->l_nco);
	/* TODO (low priority since memory gets freed in the end anyway) */
	return 0;
}


static int mod_fsk_set_symbol_source(void *arg, symbol_source_t callback, void *callback_arg)
{
	struct mod_fsk *self = (struct mod_fsk *)arg;
	self->symbol_source = callback;
	self->symbol_source_arg = callback_arg;
	return 0;
}

static int mod_fsk_source_samples(void *arg, sample_t *samples, size_t maxsamples, timestamp_t timestamp)
{
	struct mod_fsk *self = (struct mod_fsk *)arg;
	//if (self->tick != NULL)
	//	self->tick(self->input_arg, timestamp);

	size_t nsamples = 0;

	const uint32_t symrate = self->symrate;
	const float freq0 = self->freq0, freq1 = self->freq1;
	bit_t *framebuf = self->frame.data;

	char transmitting = self->transmitting;
	unsigned framelen = self->framelen, framepos = self->framepos;
	uint32_t symphase = self->symphase;

	if (transmitting == 0) {
		/*
		 * Idle (check for now incoming frames)
		 */
		const timestamp_t time_end = timestamp + (timestamp_t)(self->sample_ns * maxsamples);
		int ret = -1; // self->symbol_source(self->symbol_source_arg, &self->frame, time_end);
		if (ret > 0) {
			assert(ret <= FRAMELEN_MAX);
			transmitting = 1;
			framelen = ret;
			framepos = 0;
		}
	}

	if (transmitting == 1) {
		/*
		 * Waiting for transmitting time
		 */
		if ((int64_t)(timestamp - self->frame.hdr.timestamp) >= 0)
			transmitting = 2;

	}

	if (transmitting == 2) {
		/*
		 * Transmitting/generating samples
		 */

		size_t si;
		for(si = 0; si < maxsamples; si++) {
			float f_in = freq0; // cf  - deviation * s;
			if(framepos < framelen) {
				if(framebuf[framepos]) f_in = freq1;
			} else {
				transmitting = 0;
				break;
			}

			nco_crcf_set_frequency(self->l_nco, f_in);
			nco_crcf_step(self->l_nco);
			nco_crcf_cexpf(self->l_nco, &samples[si]);

			uint32_t symphase1 = symphase;
			symphase = symphase1 + symrate;
			if(symphase < symphase1) { // wrapped around?
				framepos++;
			}
		}
		nsamples = si;
	}

	self->transmitting = transmitting;
	self->framelen = framelen;
	self->framepos = framepos;
	self->symphase = symphase;
	return nsamples;
}


const struct mod_fsk_conf mod_fsk_defaults = {
	.samplerate = 1e6,
	.symbolrate = 9600,
	.bits_per_symbol = 2,
	.centerfreq = 100000,
	.modindex = 0.5,
	.bt = 0.5
};


CONFIG_BEGIN(mod_fsk)
CONFIG_F(samplerate)
CONFIG_F(symbolrate)
CONFIG_I(bits_per_symbol)
CONFIG_F(centerfreq)
CONFIG_F(modindex)
CONFIG_F(bt)
CONFIG_END()

const struct transmitter_code mod_fsk_code = {
	.name = "mod_fsk",
	.init = mod_fsk_init,
	.destroy = mod_fsk_destroy,
	.init_conf = init_conf, // Generate by the CONFIG-macro
	.set_conf = set_conf, // Generate by the CONFIG-macro
	.set_symbol_source = mod_fsk_set_symbol_source,
	.source_samples = mod_fsk_source_samples
};
