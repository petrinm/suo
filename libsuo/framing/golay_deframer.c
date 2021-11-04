#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <liquid/liquid.h>

#include "framing/golay_deframer.h"
#include "framing/golay24.h"
#include "suo_macros.h"


struct golay_deframer {
	/* Configuration */
	struct golay_deframer_conf c;
	uint64_t sync_mask;

	struct frame* frame;
	byte_t* data_buffer;
	unsigned int frame_pos;
	unsigned int frame_len;

	unsigned int state;
	unsigned int latest_bits;
	unsigned int bit_idx;

	unsigned int coded_len;

	SUO_CALLBACK(bit_sink);
	SUO_CALLBACK(frame_source);

	CALLBACK(frame_callback_t, frame_sink);

	fec l_rs;
	fec l_viterbi;
};


#define RANDOMIZER_LEN 256
extern const uint8_t ccsds_randomizer[RANDOMIZER_LEN];


#define MAX_FRAME_LENGTH  255
#define RS_LEN      32


static void* golay_deframer_init(const void* conf_v) {

	/* Initialize state and copy configuration */
	struct golay_deframer *self = (struct golay_deframer*)calloc(1, sizeof(struct golay_deframer));
	self->c = *(const struct golay_deframer_conf *)conf_v;

	self->sync_mask = (1ULL << self->c.sync_len) - 1;
	self->data_buffer = malloc(2 * MAX_FRAME_LENGTH);

	self->frame = suo_frame_new(1024);

	if (self->c.skip_rs == 0)
		self->l_rs = fec_create(LIQUID_FEC_RS_M8, NULL);

	if (self->c.skip_viterbi == 0) {
		self->l_viterbi = fec_create(LIQUID_FEC_CONV_V27, NULL);
		//self->viterbi_buffer = malloc(2 * MAX_FRAME_LENGTH);
	}

	return self;
}


static int golay_deframer_destroy(void *arg)  {
	struct golay_deframer *self = (struct golay_deframer*)arg;
	free(self->data_buffer);
	//free(self->viterbi_buffer);
	if (self->l_rs)
		fec_destroy(self->l_rs);
	if (self->l_viterbi)
		fec_destroy(self->l_viterbi);
	free(self);
	return 0;
}

static int golay_deframer_reset(void *arg)  {
	struct golay_deframer *self = (struct golay_deframer*)arg;
	self->state = 0;
	self->latest_bits = 0;
}

static int golay_deframer_execute(void *arg, symbol_t bit, timestamp_t time)
{
	struct golay_deframer *self = (struct golay_deframer *)arg;

	if (self->state == 0) {
		/*
		 * Looking for syncword
		 */

		self->latest_bits = (self->latest_bits << 1) | bit;

		unsigned int sync_errors = __builtin_popcountll((self->latest_bits & self->sync_mask) ^ self->c.sync_word);

		if (sync_errors <= self->c.sync_threshold) {
			printf("SYNC FOUND! %d\n", sync_errors);

			/* Syncword found, start saving bits when next bit arrives */
			self->bit_idx = 0;
			self->latest_bits = 0;

			//
			self->frame->hdr.timestamp = time;
			SET_METADATA_UI(self->frame, METADATA_SYNC_ERRORS, sync_errors);

			self->state = 1;
			return 1;
		}

		return 0;

	}
	else if (self->state == 1) {
		/*
		 * Receiveiving PHY header (lenght bytes)
		 */

		self->latest_bits = (self->latest_bits << 1) | bit;
		self->bit_idx++;

		if (self->bit_idx == 24) {

			self->coded_len = self->latest_bits;
			int golay_errors = decode_golay24(&self->coded_len);
			if (golay_errors < 0) {
				// TODO: Increase some counter
				self->state = 0;
				return 0;
			}

			// The 8 least signigicant bit indicate the lenght
			self->frame_len = 0xFF & self->coded_len;

			// Sanity
			if ((self->frame_len & 0x00) != 0) {
				self->state = 0;
				return 0;
			}

			SET_METADATA_UI(self->frame, METADATA_GOLAY_CODED, self->coded_len);
			SET_METADATA_UI(self->frame, METADATA_GOLAY_ERRORS, golay_errors);

			// Receive double number of bits if viterbi is used
			if ((self->coded_len & 0x800) != 0)
				self->frame_len *= 2;

			// Clear for next state
			self->frame_pos = 0;
			self->latest_bits = 0;
			self->bit_idx = 0;
			self->state = 2;
		}
		return 1;

	}
	else if (self->state == 2) {
		/*
		 * Receiving payload
		 */

 		self->latest_bits = (self->latest_bits << 1) | bit;
 		self->bit_idx++;

		if (self->bit_idx >= 8) {
			printf("%02x  ", self->latest_bits);

			self->frame->data[self->frame_pos] = self->latest_bits;
			self->latest_bits = 0;
			self->frame_pos++;
			self->bit_idx = 0;


			if (self->frame_pos >= self->frame_len) {
				self->frame->data_len = self->frame_len;
				self->state = 0;

				if (self->c.skip_viterbi == 0 && (self->coded_len & 0x800) != 0) {
					/* Decode viterbi */
					return suo_error(SUO_ERR_NOT_IMPLEMENTED, "Viterbi not implemented");
					// TODO
					// fec_decode(self->l_viterbi, ndec, self->buf, out->data);
					// self->frame_len /= 2;
				}

				if (self->c.skip_randomizer == 0 && (self->coded_len & 0x400) != 0) {
					/* Scrambler the bytes */
					for (size_t i = 0; i < self->frame->data_len; i++)
						self->frame->data[i] ^= ccsds_randomizer[i];
				}

				if (self->c.skip_rs == 0 && (self->coded_len & 0x200) != 0) {
					/* Decode Reed-Solomon */
					return suo_error(SUO_ERR_NOT_IMPLEMENTED, "Reed-Solomon not implemented");
#if 0
					/* Hmm, liquid-dsp doesn't return whether decode succeeded or not :(  */
					fec_decode(self->l_rs, ndec, self->buf, out->data);


					/* Re-encode it in order to calculate BER.
					 * If the number of differing octets exceeds 16, assume decoding
					 * has failed, since 32 parity bytes can't correct more than that.
					 * Not sure if this is a very reliable way to detect a failure. */
					fec_encode(self->l_rs, ndec, out->data, self->buf2);

					size_t bit_errors = 0, octet_errors = 0;
					for(size_t i = 0; i < n; i++) {
						uint8_t v1 = self->buf[i], v2 = self->buf2[i];
						bit_errors += __builtin_popcount(v1 ^ v2);
						if(v1 != v2) octet_errors++;
					}

					SET_METADATA_I(self->frame, METADATA_RS_BIT_ERRORS, bit_errors);
					SET_METADATA_I(self->frame, METADATA_RS_OCTET_ERRORS, octet_errors);
#endif
				}


				int ret = self->frame_sink(self->frame_sink_arg, self->frame, time);
				if (ret < 0) return ret;

				return 0;
			}

			return 1;
		}

	}

	return 0;
}


static int golay_deframer_set_frame_sink(void *arg, frame_callback_t callback, void *callback_arg) {
	struct golay_deframer *self = (struct golay_deframer *)arg;

	self->frame_sink = callback;
	self->frame_sink_arg = callback_arg;
	return 0;
}


const struct golay_deframer_conf golay_deframer_defaults = {
	.sync_word = 0xC9D08A7B,
	.sync_len = 32,
	.sync_threshold = 3,
	.skip_viterbi = 0,
	.skip_randomizer = 0,
	.skip_rs = 0,
};


CONFIG_BEGIN(golay_deframer)
CONFIG_I(sync_word)
CONFIG_I(sync_len)
CONFIG_I(sync_threshold)
CONFIG_I(skip_viterbi)
CONFIG_I(skip_randomizer)
CONFIG_I(skip_rs)
CONFIG_END()


extern const struct decoder_code golay_deframer_code = {
	.name = "golay_deframer",
	.init = golay_deframer_init,
	.destroy = golay_deframer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.set_frame_sink = golay_deframer_set_frame_sink,
	.execute = golay_deframer_execute,
};
