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
	struct frame frame;

	/* Allocate space for flexible array member */
	bit_t frame_buffer[FRAMELEN_MAX];
};


static void* mod_gmsk_init(const void *conf_v)
{
	struct mod_fsk *self = malloc(sizeof(struct mod_fsk));
	if(self == NULL) return NULL;
	memset(self, 0, sizeof(struct mod_fsk));

	self->c = *(const struct mod_gmsk_conf*)conf_v;
	self->sample_ns = 1.0e9f / samplerate;

	const float samples_per_symbol = self->c.samplerate / self->c.symbolrate;
	self->mod_rate = (unsigned int)samples_per_symbol + 1;
	self->mod_max_samples = (unsigned int)ceil(samples_per_symbol);
	const float resamp_rate = (floor(samples_per_symbol) + 1) / samples_per_symbol;

	/*
	 * Init GMSK modulator
	 */
	self->mod = gmskmod_create(self->mod_rate, 31, conf.bt);

	/*
	 * Init resampler
	 * The GMSK demodulator can produce signal samples/symbol ratio is an integer.
	 * This ratio doesn't usually match with the SDR's output sample ratio and thus fractional
	 * resampler is needed.
	 */
	unsigned int h_len = 13;    // filter semi-length (filter delay)
	float r=0.9f;               // resampling rate (output/input)
	float bw=0.5f;              // resampling filter bandwidth
	float slsl=-60.0f;          // resampling filter sidelobe suppression level
	unsigned int npfb=32;       // number of filters in bank (timing resolution)
	self->l_resamp = resamp_crcf_create(resamp_rate, h_len, bw, slsl, npfb);

	/*
	 * Init NCO for up mixing
	 */
	self->l_nco = nco_crcf_create(LIQUID_NCO);

	return self;
}


static int mod_gmsk_destroy(void *arg)
{
	struct mod_fsk *self = arg;
	gmskmod_destroy(self->l_mod);
	nco_crcf_destroy(self->l_nco);
	resamp_crcf_destroy(self->l_resamp);
	return 0;
}


#define SUO_CLBK_FREQUENCY  0
#define SUO_CLBK_INPUT      1

static int set_callbacks(void *arg, unsigned int callback, const struct tx_input_code *input, void *input_arg)
{
	struct mod_fsk *self = arg;
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
	struct mod_fsk *self = arg;
	self->input.tick(self->input_arg, timestamp);

	size_t nsamples = 0;

	const uint32_t symrate = self->symrate;
	bit_t *framebuf = self->frame.data;

	char transmitting = self->transmitting;
	unsigned framelen = self->framelen, framepos = self->framepos;
	uint32_t symphase = self->symphase;

	if (!transmitting) {
		const timestamp_t time_end = timestamp + (timestamp_t)(self->sample_ns * maxsamples);
		int ret = self->input.get_frame(self->input_arg, &self->frame, FRAMELEN_MAX, time_end);
		if (ret > 0) {
			assert(ret <= FRAMELEN_MAX);
			transmitting = 1;
			framelen = ret;
			framepos = 0;
		}
	}

	if (transmitting == 1 && (int64_t)(timestamp - self->frame.m.time) >= 0)
		transmitting = 2;

	if (transmitting == 2) {

		sample_t sym_samples[self->mod_max_samples];
		unsigned int max_symbols = maxsamples / OVERSAMPLING;
		for(size_t si = 0; si < max_symbols; si++) {

			unsigned int symbol = frame[framepos++];
			gmskmod_modulate(self->l_mod, symbol, &sym_samples[si]);

			unsigned int num_written;
			resamp_crcf_execute(self->l_resamp, self->mod_rate, y, &num_written);

			// Mix up the samples
			nco_crcf_mix_block_up(sym_samples, samples[i], num_written);


		}
		nsamples = si;
	}

	self->transmitting = transmitting;
	self->framelen = framelen;
	self->framepos = framepos;
	self->symphase = symphase;
	return (tx_return_t){ .len = maxsamples, .begin=0, .end = nsamples };
}


const struct mod_gmsk_conf mod_gmsk_defaults = {
	.samplerate = 1e6,
	.symbolrate = 9600,
	.centerfreq = 100000,
	.modindex = 0.5,
	.bt = 0.5
};


CONFIG_BEGIN(mod_gmsk)
CONFIG_F(samplerate)
CONFIG_F(symbolrate)
CONFIG_F(centerfreq)
CONFIG_F(bt)
CONFIG_END()

const struct transmitter_code mod_fsk = {
	.name = "mod_gmsk",
	.init = mod_gmsk_init,
	.destroy = mod_gmsk_destroy,
	.init_conf = init_conf, // Generate by the CONFIG-macro
	.set_conf = set_conf, // Generate by the CONFIG-macro
	.set_callbacks = set_callbacks,
	.execute = mod_gmsk_execute
};
