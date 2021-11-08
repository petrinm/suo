#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framing/golay_framer.h"
#include "framing/golay24.h"
#include "framing/utils.h"

#include "suo_macros.h"

#include <liquid/liquid.h>

struct golay_framer {
	/* Configuration */
	struct golay_framer_conf c;

	/* State */
	int state;
	int bit_idx;

	/* Buffers */
	byte_t* data_buffer;
	byte_t* viterbi_buffer;
	bit_t* bit_buffer;
	struct frame *frame;

	/* Liquid-DSP FECs */
	fec l_rs;
	fec l_viterbi;

	/* Callbacks */
	CALLBACK(frame_source_t, frame_source);
};


/*
 * The pseudo random sequence generated using the polynomial  h(x) = x8 + x7 + x5 + x3 + 1.
 * Ref: CCSDS 131.0-B-3, 10.4.1
 */
#define RANDOMIZER_LEN   256
const uint8_t ccsds_randomizer[RANDOMIZER_LEN] = {
	0xff, 0x48, 0x0e, 0xc0, 0x9a, 0x0d, 0x70, 0xbc,
	0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce,
	0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e,
	0x0a, 0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1,
	0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79,
	0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c,
	0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c,
	0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63,
	0xfd, 0x20, 0x3b, 0x02, 0x68, 0x35, 0xc2, 0xf2,
	0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39,
	0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8,
	0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7,
	0xfa, 0x40, 0x76, 0x04, 0xd0, 0x6b, 0x85, 0xe4,
	0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72,
	0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0,
	0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f,
	0xf4, 0x80, 0xec, 0x09, 0xa0, 0xd7, 0x0b, 0xc8,
	0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5,
	0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0,
	0xa1, 0x0f, 0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f,
	0xe9, 0x01, 0xd8, 0x13, 0x41, 0xae, 0x17, 0x91,
	0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb,
	0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1,
	0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f,
	0xd2, 0x03, 0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23,
	0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96,
	0xa5, 0xdf, 0x73, 0x0c, 0xa8, 0xaf, 0xcf, 0x82,
	0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f,
	0xa4, 0x07, 0x60, 0x4d, 0x06, 0xb8, 0x5e, 0x47,
	0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d,
	0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x05,
	0x08, 0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff
};

#define MAX_FRAME_LENGTH  255
#define RS_LEN      32


static void* golay_framer_init(const void* conf_v) {
	/* Initialize state and copy configuration */
	struct golay_framer *self = (struct golay_framer *)malloc(sizeof(struct golay_framer));
	memset(self, 0, sizeof(struct golay_framer));
	self->c = *(const struct golay_framer_conf *)conf_v;

	self->frame = suo_frame_new(256);

	self->data_buffer = (uint8_t*)malloc(MAX_FRAME_LENGTH);

	if (self->c.use_rs)
		self->l_rs = fec_create(LIQUID_FEC_RS_M8, NULL);

	if (self->c.use_viterbi) {
		self->l_viterbi = fec_create(LIQUID_FEC_CONV_V27, NULL);
		self->viterbi_buffer = (uint8_t*)malloc(2 * MAX_FRAME_LENGTH);
	}

	return self;
}


static int golay_framer_destroy(void *arg) {
	struct golay_framer *self = (struct golay_framer*)arg;
	free(self->data_buffer);
	if (self->viterbi_buffer)
		free(self->viterbi_buffer);
	if (self->l_rs)
		fec_destroy(self->l_rs);
	if (self->l_viterbi)
		fec_destroy(self->l_viterbi);
	free(self);
	return 0;
}



static int golay_framer_set_frame_source(void *arg, frame_source_t callback, void *callback_arg) {
	struct golay_framer *self = (struct golay_framer*)arg;
	self->frame_source = callback;
	self->frame_source_arg = callback_arg;
	return SUO_OK;
}


static int golay_framer_source_symbols(void *arg, symbol_t* symbols, size_t max_symbols, timestamp_t t) {
	struct golay_framer *self = (struct golay_framer*)arg;
	if (self->frame_source == NULL || self->frame_source_arg == NULL)
		return suo_error(SUO_ERROR, "No frame source defined!");

	// Get a frame
	int ret = self->frame_source(self->frame_source_arg, self->frame, 0);
	if (ret <= 0)
		return ret;

	struct frame *frame = self->frame;

	/* Start processing a frame */
	unsigned int payload_len = frame->data_len;
	if (self->c.use_rs)
	 	payload_len += RS_LEN;

	if (payload_len >= MAX_FRAME_LENGTH)
		return suo_error(-9999, "Buffer overrun!");

	/* Generate preamble sequence */
	bit_t *bit_ptr = symbols;
	for (unsigned int i = 0; i < self->c.preamble_len; i++)
		*bit_ptr++ = i & 1;


	/* Append syncword */
	bit_ptr += word_to_lsb_bits(bit_ptr, self->c.sync_len, self->c.sync_word);

	/* Append Golay coded length (+coding flags) */
	uint32_t coded_len = self->c.golay_flags | payload_len;
	if (self->c.use_rs)
		coded_len |= 0x200;
	if (self->c.use_randomizer)
		coded_len |= 0x400;
	if (self->c.use_viterbi)
		coded_len |= 0x800;

	encode_golay24(&coded_len);
	bit_ptr += word_to_lsb_bits(bit_ptr, 24, coded_len);

	/* Calculate Reed Solomon */
	if (self->c.use_rs) {
		fec_encode(self->l_rs, frame->data_len, (unsigned char*)frame->data, self->data_buffer);
	}
	else {
		memcpy(self->data_buffer, frame->data, frame->data_len);
	}

	/* Scrambler the bytes */
	if (self->c.use_randomizer) {
		for (size_t i = 0; i < frame->data_len; i++)
			self->data_buffer[i] ^= ccsds_randomizer[i];
	}

	/* Viterbi decode all bits */
	if (self->c.use_viterbi) {
		fec_encode(self->l_viterbi, frame->data_len, self->data_buffer, self->viterbi_buffer);
		bit_ptr += bytes_to_bits(bit_ptr, self->viterbi_buffer, 2 * payload_len, 1);
	}
	else {
		bit_ptr += bytes_to_bits(bit_ptr, self->data_buffer, payload_len, 1);
	}

	/* Tail bits */
	for (unsigned int i = 0; i < self->c.tail_length; i++)
		*(bit_ptr++) = (rand() & 1);

	size_t total_bits = (size_t)(bit_ptr - symbols);
	if (total_bits >= max_symbols)
		return suo_error(-9999, "Buffer overrun!");

#if 1n
	printf("OUTPUT SAMPLES:\n");
	suo_print_symbols(symbols, total_bits);
#endif

	return total_bits;
}


const struct golay_framer_conf golay_framer_defaults = {
	.sync_word = 0xC9D08A7B,
	.sync_len = 32,
	.preamble_len = 0x50,
	.use_viterbi = 0,
	.use_randomizer = 1,
	.use_rs = 1,
	.tail_length = 0,
	.golay_flags = 0,
};


CONFIG_BEGIN(golay_framer)
CONFIG_I(sync_word)
CONFIG_I(sync_len)
CONFIG_I(preamble_len)
CONFIG_I(use_viterbi)
CONFIG_I(use_randomizer)
CONFIG_I(use_rs)
CONFIG_I(tail_length)
CONFIG_I(golay_flags)
CONFIG_END()


const struct encoder_code golay_framer_code = {
	.name = "golay_framer",
	.init = golay_framer_init,
	.destroy = golay_framer_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.set_frame_source = golay_framer_set_frame_source,
	.source_symbols = golay_framer_source_symbols,
};
