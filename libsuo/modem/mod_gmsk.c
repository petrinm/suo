#include "mod_gmsk.h"
#include "suo_macros.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <liquid/liquid.h>

#define FRAMELEN_MAX 0x900
static const float pi2f = 6.283185307179586f;

struct mod_gmsk {

	/* Configuration */
	struct mod_gmsk_conf c;
	uint32_t symrate; // integer symbol rate
	float sample_ns; // Sample duration in ns

	/* State */
	char transmitting; // 0 = no frame, 1 = frame waiting, 2 = transmitting
	unsigned framelen, framepos;
	uint32_t symphase;
	unsigned int mod_rate;
	unsigned int mod_max_samples;

	/* liquid-dsp objects */
	nco_crcf l_nco;
	resamp_crcf l_resamp;
	gmskmod l_mod;

	/* Callbacks */
	struct tx_input_code input;
	void *input_arg;

	/* Buffers */
	struct frame* frame;

};


static void* mod_gmsk_init(const void *conf_v)
{
	struct mod_gmsk *self = malloc(sizeof(struct mod_gmsk));
	if(self == NULL) return NULL;
	memset(self, 0, sizeof(struct mod_gmsk));

	self->c = *(const struct mod_gmsk_conf*)conf_v;
	self->sample_ns = 1.0e9f / self->c.samplerate;

	const float samples_per_symbol = self->c.samplerate / self->c.symbolrate;
	self->mod_rate = (unsigned int)samples_per_symbol + 1;
	self->mod_max_samples = (unsigned int)ceil(samples_per_symbol);
	const float resamp_rate = (floor(samples_per_symbol) + 1) / samples_per_symbol;

	assert(self->mod_rate >= 4);

	/*
	 * Init GMSK modulator
	 */
	self->l_mod = gmskmod_create(self->mod_rate, 31, self->c.bt);

	/*
	 * Init resampler
	 * The GMSK demodulator can produce signal samples/symbol ratio is an integer.
	 * This ratio doesn't usually match with the SDR's output sample ratio and thus fractional
	 * resampler is needed.
	 */
	unsigned int h_len = 13;    // filter semi-length (filter delay)
	float bw = 0.5f;            // resampling filter bandwidth
	float slsl = -60.0f;        // resampling filter sidelobe suppression level
	unsigned int npfb = 32;     // number of filters in bank (timing resolution)
	self->l_resamp = resamp_crcf_create(resamp_rate, h_len, bw, slsl, npfb);

	/*
	 * Init NCO for up mixing
	 */
	self->l_nco = nco_crcf_create(LIQUID_NCO);

	return self;
}


static int mod_gmsk_destroy(void *arg)
{
	struct mod_gmsk *self = arg;
	gmskmod_destroy(self->l_mod);
	nco_crcf_destroy(self->l_nco);
	resamp_crcf_destroy(self->l_resamp);
	return 0;
}


#define SUO_CLBK_FREQUENCY  0
#define SUO_CLBK_INPUT      1

static int set_callbacks(void *arg, unsigned int callback, const struct tx_input_code *input, void *input_arg)
{
	struct mod_gmsk *self = arg;
	if (callback == SUO_CLBK_FREQUENCY) {
		float f = 0.1;
		nco_crcf_set_frequency(self->l_nco, f);
		// TODO
		return 0;
	}
	if (callback == SUO_CLBK_INPUT) {
		self->input = *input;
		self->input_arg = input_arg;
		return 0;
	}
	return -1; // No such
}


static tx_return_t mod_gmsk_execute(void *arg, sample_t *samples, size_t max_samples, timestamp_t timestamp)
{
	struct mod_gmsk *self = arg;
	self->input.tick(self->input_arg, timestamp);

	size_t nsamples = 0;

	if (self->transmitting == 0) {
		/*
		 * Idle (check for now incoming frames)
		 */
		const timestamp_t time_end = timestamp + (timestamp_t)(self->sample_ns * max_samples);
		int ret = self->input.get_frame(self->input_arg, &self->frame, FRAMELEN_MAX, time_end);
		if (ret > 0) {
			assert(ret <= FRAMELEN_MAX);
			self->transmitting = 1;
			self->framelen = ret;
			self->framepos = 0;
		}
	}

	if (self->transmitting == 1) {
		/*
		 * Waiting for transmitting time
		 */
		if ((int64_t)(timestamp - self->frame->timestamp) >= 0)
			self->transmitting = 2;
	}

	if (self->transmitting == 2) {
		/*
		 * Transmitting/generating samples
		 */

		sample_t sym_samples[self->mod_max_samples];
		unsigned int max_symbols = max_samples / self->mod_rate;
		bit_t *framebuf = self->frame->data;

		size_t si = 0;
		for(; si < max_symbols; si++) {

			unsigned int symbol = framebuf[self->framepos++];
			gmskmod_modulate(self->l_mod, symbol, &sym_samples[si]);

			unsigned int num_written;
			//resamp_crcf_execute(self->l_resamp, self->mod_rate, y, &num_written);

			// Mix up the samples
			//nco_crcf_mix_block_up(self->l_nco, sym_samples, samples[i], num_written);

		}
		nsamples = si;
	}

	return (tx_return_t){ .len = max_samples, .begin=0, .end = nsamples };
}


const struct mod_gmsk_conf mod_gmsk_defaults = {
	.samplerate = 1e6,
	.symbolrate = 9600,
	.centerfreq = 100000,
	.bt = 0.5
};


CONFIG_BEGIN(mod_gmsk)
CONFIG_F(samplerate)
CONFIG_F(symbolrate)
CONFIG_F(centerfreq)
CONFIG_F(bt)
CONFIG_END()

const struct transmitter_code mod_gmsk_code = {
	.name = "mod_gmsk",
	.init = mod_gmsk_init,
	.destroy = mod_gmsk_destroy,
	.init_conf = init_conf, // Generate by the CONFIG-macro
	.set_conf = set_conf, // Generate by the CONFIG-macro
	.set_callbacks = set_callbacks,
	.execute = mod_gmsk_execute
};
