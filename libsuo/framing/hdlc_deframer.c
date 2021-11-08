#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hdlc_deframer.h"
#include "suo_macros.h"

struct hdlc_deframer {
	/* Configuration */
	struct hdlc_deframer_conf c;

	/* State */
	enum {
		STATE_RX_SYNC = 0,
		STATE_RX_PAYLOAD
	} state;
	unsigned int shift;
	unsigned int bit_idx;

	/* Buffers */
	struct frame* frame;

	/* Callbacks */
	CALLBACK(frame_source_t, frame_source);
};


static void* hdlc_deframer_init(const void* conf_v) {
	struct hdlc_deframer* self = (struct hdlc_deframer*)calloc(1, sizeof(struct hdlc_deframer));
	self->c = *(struct hdlc_deframer_conf*)conf_v;

	self->state = STATE_RX_SYNC;
	//hdlc_deframer_reset(self);

	return self;
}


static int hdlc_deframer_destroy(void *arg) {
	struct hdlc_deframer *self = (struct hdlc_deframer*)arg;
	suo_frame_destroy(self->frame);
	self->frame = NULL;
	free(self);
	return SUO_OK;
}

static int hdlc_deframer_reset(void *arg) {
	struct hdlc_deframer *self = (struct hdlc_deframer*)arg;
	self->state = STATE_RX_SYNC;
	self->shift = 0;
	self->bit_idx = 0;
	return SUO_OK;
}


static int hdlc_deframer_set_frame_source(void *arg, frame_source_t callback, void *callback_arg) {
	struct hdlc_deframer *self = (struct hdlc_deframer*)arg;
	self->frame_source = callback;
	self->frame_source_arg = callback_arg;
	return SUO_OK;
}


static int hdlc_deframer_sink_symbol(void* arg, bit_t bit, timestamp_t now) {
	struct hdlc_deframer* self = (struct hdlc_deframer*)arg;
	(void)bit; (void)now;
	return SUO_OK;
	// Execute
#if 0
	if (0) {
		/* G3RUH decode */
		lastbit =
	}
	else if (1) {
		/* NRZI decode */
		bit_t new_bit = !(bit ^ rx->lastbit);
		rx->lastbit = bit;
		bit = new_bit;
	}
	else if (1) {
		/* NRZ decode */
		bit_t new_bit = (bit ^ rx->lastbit);
		rx->lastbit = bit;
		bit = new_bit;
	}


	self->shift = (0xFF & (self->shift << 1)) | bit;


	static bool stuffed;


	if (state == STATE_RX_SYNC) {
		/*
		 * Looking for start byte
		 */

		if (shift == 0x7E) {
			return 1;
	 	}

		return 0;
	}
	else if (state == STATE_RX_PAYLOAD) {
		/*
		 * Frame
		 */

		if (stuffed) {

			if (bit == 1) {
				// End flag
				state = stop;
				ndata = 0;
			}
			else {

				stuffed = false;
			}

			state = STATE_RX_SYNC;
		}
		else {


			stuffed = true;
		}
	}
#endif
}


const struct hdlc_deframer_conf hdlc_deframer_defaults = {
	.mode = 0,
};

CONFIG_BEGIN(hdlc_deframer)
CONFIG_I(mode)
CONFIG_END()

const struct decoder_code hdlc_deframer_code = {
	.name = "hdlc_deframer",
	.init = hdlc_deframer_init,
	.destroy = hdlc_deframer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.sink_symbol = hdlc_deframer_sink_symbol,
};
