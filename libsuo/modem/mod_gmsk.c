
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "suo.h"
#include "suo_macros.h"
#include "mod_gmsk.h"

#include <liquid/liquid.h>


#define MAX_SYMBOLS   0x900

static const float pi2f = 6.283185307179586f;

struct mod_gmsk {
	/* Configuration */
	struct mod_gmsk_conf c;
	uint32_t        symrate; // integer symbol rate
	timestamp_t     sample_ns; // Sample duration in ns
	unsigned int    mod_rate; // GMSK modulator samples per symbols rate
	unsigned int    mod_max_samples; // Maximum number of samples generated from single symbol
	float           nco_1Hz;

	/* State */
	enum {
		IDLE = 0,
		WAITING,
		TRANSMITTING,
	} state;

	symbol_t*       symbols;  // Symbol buffer
	unsigned int    symbols_len; // Symbol buffer length
	unsigned int    symbols_i; // Symnol buffer index

	/* Liquid-DSP objects */
	gmskmod         l_mod; // GMSK modulator
	resamp_crcf     l_resamp; // Rational resampler
	nco_crcf        l_nco; // NCO for mixing up the signal

	/* Callbacks */
	CALLBACK(symbol_source_t, symbol_source);
	CALLBACK(tick_sink_t, tick);
};


static void* mod_gmsk_init(const void *conf_v)
{
	struct mod_gmsk *self = (struct mod_gmsk *)malloc(sizeof(struct mod_gmsk));
	if(self == NULL) return NULL;
	memset(self, 0, sizeof(struct mod_gmsk));

	self->c = *(const struct mod_gmsk_conf*)conf_v;
	self->sample_ns = round(1.0e9 / self->c.samplerate);
	self->nco_1Hz = pi2f / self->c.samplerate;

	const float samples_per_symbol = self->c.samplerate / self->c.symbolrate;
	self->mod_rate = (unsigned int)samples_per_symbol + 1;
	self->mod_max_samples = (unsigned int)ceil(samples_per_symbol);
	const float resamp_rate = samples_per_symbol / (float)self->mod_rate;

	printf("# mod_gmsk_init\n");
	printf("samples_per_symbol = %f\n", samples_per_symbol);
	printf("mod_rate = %d\n", self->mod_rate);
	printf("resamp_rate = %f\n", resamp_rate);

	assert(self->mod_rate >= 4);

	/*
	 * Buffers
	 */
	self->symbols = calloc(sizeof(sample_t), MAX_SYMBOLS);
	self->symbols_i = 0;

	/*
	 * Init GMSK modulator
	 */
	self->l_mod = gmskmod_create(self->mod_rate, 15, self->c.bt);



	/*
	 * Init resampler
	 * The GMSK demodulator can produce signal samples/symbol ratio is an integer.
	 * This ratio doesn't usually match with the SDR's output sample ratio and thus fractional
	 * resampler is needed.
	 */
	self->l_resamp = resamp_crcf_create(resamp_rate,
		13,    /* filter semi-length (filter delay) */
		0.4f,  /* resampling filter bandwidth */
		60.0f, /* resampling filter sidelobe suppression level */
		32     /* number of filters in bank (timing resolution) */
	);

	/*
	 * Init NCO for up mixing
	 */
	self->l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_set_frequency(self->l_nco, self->nco_1Hz * self->c.centerfreq);

	return self;
}


static int mod_gmsk_reset(void *arg) {
	struct mod_gmsk *self = (struct mod_gmsk *)arg;
	self->state = IDLE;
	self->symbols_len = 0;
	self->symbols_i = 0;

	return SUO_OK;
}


static int mod_gmsk_destroy(void *arg)
{
	struct mod_gmsk *self = (struct mod_gmsk *)arg;
	gmskmod_destroy(self->l_mod);
	resamp_crcf_destroy(self->l_resamp);
	nco_crcf_destroy(self->l_nco);
	free(self);
	return 0;
}


#if 0
static int set_callbacks(void *arg, unsigned int callback, const struct tx_input_code *input, void *input_arg)
{
	struct mod_gmsk *self = (struct mod_gmsk *)arg;
	if (callback == SUO_CLBK_FREQUENCY) {
		float f = 0.1;
		nco_crcf_set_frequency(self->l_nco, f);
		// TODO
		return 0;
	}
	return -1; // No such
}
#endif


static int mod_gmsk_set_symbol_source(void *arg, symbol_source_t callback, void *callback_arg) {
	struct mod_gmsk *self = (struct mod_gmsk *)arg;
	self->symbol_source = callback;
	self->symbol_source_arg = callback_arg;
	return 0;
}


static int mod_gmsk_source_samples(void *arg, sample_t *samples, size_t max_samples, timestamp_t timestamp)
{
	struct mod_gmsk *self = (struct mod_gmsk *)arg;

	if (self->symbol_source == NULL && self->symbol_source_arg == NULL)
	 	return suo_error(SUO_ERROR, "No transmitter frame input defined");

	//if (self->symbol_source.tick)
	//	self->symbol_source.tick(self->symbol_source_arg, timestamp);

	size_t nsamples = 0;

	if (self->state == IDLE) {
		/*
		 * Idle (check for now incoming frames)
		 */
		const timestamp_t time_end = timestamp + (timestamp_t)(self->sample_ns * max_samples);
		int ret = self->symbol_source(self->symbol_source_arg, self->symbols, MAX_SYMBOLS, time_end);
		//int ret = self->symbol_source.get(self->);

		if (ret > 0) {
			if (ret >= MAX_SYMBOLS)
				return suo_error(SUO_ERROR, "!");

			self->state = WAITING;
			self->symbols_len = ret;
			self->symbols_i = 0;
		}
	}

	if (self->state == WAITING) {
		/*
		 * Waiting for transmitting time
		 */
		//if ((int64_t)(timestamp - self->frame->hdr.timestamp) >= 0)
			self->state = TRANSMITTING;
	}

	if (self->state == TRANSMITTING) {
		/*
		 * Transmitting/generating samples
		 */

		sample_t mod_samples[self->mod_rate];
		unsigned int max_symbols = max_samples / self->mod_max_samples;
		if (max_symbols == 0)
			return suo_error(-999, "BAD!");

		size_t si = 0;
		for(; si < max_symbols; si++) {

			// Generate samples from the symbol
			unsigned int symbol = self->symbols[self->symbols_i++];
			gmskmod_modulate(self->l_mod, symbol, mod_samples);

			// Interpolate to final sample rate
			unsigned int num_written = 0;
			sample_t *si = mod_samples, *so = samples;
			for (unsigned int i = 0; i < self->mod_rate; i++) {
				unsigned int more;
				resamp_crcf_execute(self->l_resamp, *(si++), so, &more);
				so += more;
				num_written += more;
			}

			// Mix up the samples
			nco_crcf_mix_block_up(self->l_nco, samples, samples, num_written);

#if 0 // TODO
			// Start ramp up
			if (symbols_i < 0) {
				for (unsigned int i = 0; i < num_written; i++)
					samples[i] *= liquid_hamming(self->symbol_counter*self->k + i, 2*self->m*self->k);
			}

			// End ramp down
			if (self->symbol_counter >= self->m)
 				new_samples[i] *= liquid_hamming(self->m*self->k + (self->symbol_counter-self->m)*self->k + i, 2*self->m*self->k);
#endif

			samples += num_written;
			nsamples += num_written;
			if (self->symbols_i == self->symbols_len) {
				self->symbols_i = 0;
				self->state = IDLE;
				break;
			}
		}

		return nsamples;
	}

	return SUO_OK;
}


const struct mod_gmsk_conf mod_gmsk_defaults = {
	.samplerate = 1e6,
	.symbolrate = 9600,
	.centerfreq = 100e3,
	.bt = 0.5,
	.ramp = 0,
};


CONFIG_BEGIN(mod_gmsk)
CONFIG_F(samplerate)
CONFIG_F(symbolrate)
CONFIG_F(centerfreq)
CONFIG_F(bt)
CONFIG_I(ramp)
//CONFIG_CALLBACK(source_sample)
//CONFIG_CALLER(sink_frames)
CONFIG_END()

const struct transmitter_code mod_gmsk_code = {
	.name = "mod_gmsk",
	.init = mod_gmsk_init,
	.destroy = mod_gmsk_destroy,
	.init_conf = init_conf, // Generate by the CONFIG-macro
	.set_conf = set_conf, // Generate by the CONFIG-macro
	.set_symbol_source = mod_gmsk_set_symbol_source,
	.source_samples = mod_gmsk_source_samples,
	//.sink_frames = 0,
};
