#if 0
#include "hdlc_deframer.h"
#include "suo_macros.h"

struct hdlc_framer {
	/* configuration */
	struct hdlc_framer_conf c;

	unsigned int shift;
	unsigned int state;
	unsigned int bit_idx;

	struct frame;
};

static void* hdlc_framer_init(void* conf_v) {
	struct hdlc_framer* self = malloc(sizeof(struct hdlc_framer));
	memset(self, 0, sizeof(struct hdlc_framer));
	self->c = (struct hdlc_framer_conf*)conf_v;


	return self;
}

static void hdlc_framer_execute(void* arg) {
	struct hdlc_framer* self = (struct hdlc_framer*)arg;
	// None
}

static void hdlc_framer_execute(unsigned int bit) {

	self->shift = (0xFF & (self->shift << 1)) | bit;

	if (state == 0) {
		/*
		 * Looking for start byte
		 */

		if (shift == 0x7E) {
			return 1;
	 	}

		return 0;
	}
	else {
		/*
		 * Frame...
		 */

	}

}


const struct hdlc_deframer_conf hdlc_deframer_defaults = {
	.mode = 0,
};

CONFIG_BEGIN(hdlc_deframer)
CONFIG_I(mode)
CONFIG_END()

extern const struct decoder_code hdlc_deframer_code = {
	.name = "hdlc_deframer",
	.init = hdlc_deframer_init,
	.destroy = hdlc_deframer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.execute = hdlc_deframer_execute,
};

#endif
