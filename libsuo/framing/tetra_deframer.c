#if 0

#include "framing/tetra_deframer.h"
#include "suo_macros.h"
#include <assert.h>
#include <stdio.h> //debug prints
#include <liquid/liquid.h>


static void* tetra_deframer_init(const void* conf_v) {
	static struct syncword_deframer *deframer_init(struct syncword_deframer_conf *conf)
	{
		deframer_t *self = malloc(sizeof(struct syncword_deframer));
		memset(self, 0, sizeof(struct syncword_deframer));


	self->syncmask1 = (1ULL << self->c.synclen1) - 1;
	self->syncmask2 = (1ULL << self->c.synclen2) - 1;
	self->syncmask3 = (1ULL << self->c.synclen3) - 1;

}

/* Check for different syncwords. */
static inline void check_sync(struct burst_dpsk_receiver *self, uint64_t lb, timestamp_t ts) {
	unsigned syncerrs;

	/* Check sync word 1 */
	syncerrs = __builtin_popcountll((lb & self->syncmask1) ^ self->c.syncword1);
	if (syncerrs <= 0) {
		fprintf(stderr, "%20lu: Found syncword 1 with %u errors\n", ts, syncerrs);
		output_frame(self, ts, 1);
	}

	/* Check sync word 3 */
	syncerrs = __builtin_popcountll((lb & self->syncmask2) ^ self->c.syncword2);
	if (syncerrs <= 0) {
		fprintf(stderr, "%20lu: Found syncword 2 with %u errors\n", ts, syncerrs);
		output_frame(self, ts, 2);
	}

	/* Check sync word 2 */
	syncerrs = __builtin_popcountll((lb & self->syncmask3) ^ self->c.syncword3);
	if (syncerrs <= 3) {
		fprintf(stderr, "%20lu: Found syncword 3 with %u errors\n", ts, syncerrs);
		output_frame(self, ts, 3);
	}
}


const struct tetra_deframer_conf tetra_deframer_defaults = {
	.syncword1 = 0b1101000011101001110100,
	.syncword2 = 0b0111101001000011011110,
	.syncword3 = 0b11000001100111001110100111000001100111,
	.synclen1 = 22,
	.synclen2 = 22,
	.synclen3 = 38,
	.syncpos = 133,
	.framelen = 255
};


CONFIG_BEGIN(tetra_deframer)
CONFIG_I(syncword1)
CONFIG_I(syncword2)
CONFIG_I(syncword3)
CONFIG_I(synclen1)
CONFIG_I(synclen2)
CONFIG_I(synclen3)
CONFIG_I(syncpos)
CONFIG_I(framelen)
CONFIG_END()


extern const struct decoder_code tetra_deframer_code = {
	.init = tetra_deframer_init,
	.destroy = tetra_deframer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.execute = tetra_deframer_execute,
};


#endif
