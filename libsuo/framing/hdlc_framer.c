#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hdlc_framer.h"
#include "suo_macros.h"

struct hdlc_framer {
	/* Configuration */
	struct hdlc_framer_conf c;

	/* State */
	unsigned int shift;

	/* Callbacks */
	CALLBACK(frame_source_t, frame_source);

};

static void* hdlc_framer_init(const void* conf_v) {
	struct hdlc_framer* self = malloc(sizeof(struct hdlc_framer));
	memset(self, 0, sizeof(struct hdlc_framer));
	self->c = *(struct hdlc_framer_conf*)conf_v;


	return self;
}


static int hdlc_framer_destroy(void *arg) {
	struct hdlc_framer *self = (struct hdlc_framer*)arg;
	free(self);
	return SUO_OK;
}


static int hdlc_framer_set_frame_source(void *arg, frame_source_t callback, void *callback_arg) {
	struct hdlc_framer *self = (struct hdlc_framer*)arg;
	self->frame_source = callback;
	self->frame_source_arg = callback_arg;
	return SUO_OK;
}


static int hdlc_framer_source_symbols(void *arg, symbol_t* symbols, size_t max_symbols, suo_timestamp_t t) {


	unsigned int stuffing = 0;

	return SUO_OK;
}




const struct hdlc_framer_conf hdlc_framer_defaults = {
	.mode = 0
};

CONFIG_BEGIN(hdlc_framer)
CONFIG_I(mode)
CONFIG_END()

const struct encoder_code hdlc_framer_code = {
	.name = "hdlc_framer",
	.init = hdlc_framer_init,
	.destroy = hdlc_framer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.set_frame_source = hdlc_framer_set_frame_source,
	.source_symbols = hdlc_framer_source_symbols,
};
