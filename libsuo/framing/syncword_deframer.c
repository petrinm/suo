#include "syncword_deframer.h"
#include <string.h>
#include <assert.h>
/*#include <stdio.h>
#include <liquid/liquid.h>*/

#if 0

typedef uint64_t bw_t; // bit window

#define SYNCWORD_EFR32 0b010101011111011010001101
#define SYNCMASK_EFR32 0b111111111111111111111111


#define SYNC_WIN_START 40
#define SYNC_WIN_END 300
#define SYNC_THRESHOLD 40
#define PKT_BITS 304
#define BITS_STORED (SYNC_WIN_END+PKT_BITS+100)

//#define CALLBACK_TYPE

struct syncword_deframer {

	/* Configuration */
	syncword_deframer_conf c;
	bw_t sync_mask;

	bw_t last_bits;
	int bit_num;
	int syncp, running, least_errs, least_errs_p;
	int syncerrs[SYNC_WIN_END];
	int pbits[BITS_STORED];

	/* callbacks */
	struct frame_output_code output;
	void *output_arg;
	void (*output_f)(void *arg, int *bits, int len);

	CALLBACK(frame_callback_t, frame_source);
};


static struct syncword_deframer *deframer_init(struct syncword_deframer_conf *conf)
{
	struct syncword_deframer *self = (struct syncword_deframer*)calloc(1, sizeof(struct syncword_deframer));

	/* TODO find neat way to change packet handler */
	self->c = *conf;
	self->pkt_arg = efrk7_init();
	self->pkt_received = efrk7_decode;

	self->sync_mask = (1ULL << conf->sync_len) - 1;

	deframer_reset(s);
	self->running = 0;

	return self;
}

static void deframer_reset(void *arg)
{
	struct syncword_deframer *self = (struct syncword_deframer*)arg;
	self->bit_num = 0;
	self->last_bits = 0;
	self->least_errs = 100;
	self->least_errs_p = 0;
	self->syncp = -1;
	self->running = 1;
}


static int deframer_set_callbacks(void *arg, const struct frame_output_code *output, void *output_arg)
{
	struct syncword_deframer *self = (struct syncword_deframer*)arg;
	self->output = *frame_output_code;
	self->output_arg = output_arg;
}


static void deframer_execute(void *arg, int b) {
	struct syncword_deframer *self = (struct syncword_deframer*)arg;

	int errs;
	if(!self->running) return;

	if(self->bit_num < BITS_STORED) {
		self->pbits[self->bit_num] = b;
	} else {
		self->running = 0;
		return;
	}

	if(self->bit_num < SYNC_WIN_END) {

		// Push the bit to the shift register
		self->last_bits = (self->last_bits << 1) | (b & 1);

		// Calculate number of errors
		errs = __builtin_popcountll((self->last_bits ^ self->c.syncword) & self->syncmask);
		self->syncerrs[self->bit_num] = errs;

		if(errs < self->least_errs) {
			self->least_errs = errs;
			self->least_errs_p = self->bit_num;
		}
	}

	if(self->least_errs <= self->c.sync_threshold && self->bit_num == self->least_errs_p+8)
		self->syncp = self->least_errs_p+1;

	if(self->syncp >= 0 && self->bit_num >= self->syncp + PKT_BITS) {
		self->pkt_received(self->pkt_arg, self->pbits + self->syncp/* -16*/, PKT_BITS);
		self->running = 0;
	}
	self->bit_num++;
}



CONFIG_BEGIN(syncword_deframer)
CONFIG_I(sync_word)
CONFIG_I(sync_len)
CONFIG_I(sync_threshold)
CONFIG_END()

const struct decoder_code syncword_deframer = {
	.name = "syncword_deframer",
	.init = deframer_init,
	.destroy = NULL,
	.reset = deframer_reset,
	.set_config = set_config,
	.execute = deframer_bit,
};
#endif
