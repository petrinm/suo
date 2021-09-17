#if 0
#include "hdlc_framer.h"
#include "suo_macros.h"

struct hdlc_framer {
	/* configuration */
	struct hdlc_framer_conf c;

	unsigned int shift;
};

static void* hdlc_framer_init(const void* conf_v) {
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


	unsigned int stuffing = 0;

}




const struct hdlc_framer_conf hdlc_framer_defaults = {
	.mode = 0
};

CONFIG_BEGIN(hdlc_framer)
CONFIG_I(mode)
CONFIG_END()

extern const struct encoder_code hdlc_framer_code = {
	.name = "hdlc_framer",
	.init = hdlc_framer_init,
	.destroy = hdlc_framer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.execute = hdlc_framer_execute,
};

#endif
