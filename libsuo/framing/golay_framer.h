#ifndef __SUO_FRAMING_GOLAY_FRAMER_H__
#define __SUO_FRAMING_GOLAY_FRAMER_H__

#include "suo.h"

struct golay_framer_conf {

	/* Syncword */
	unsigned int sync_word;

	/* Number of bits in syncword */
	unsigned int sync_len;

	/* Number of bit */
	unsigned int preamble_len;

	/* Use viterbi convolutional coding */
	unsigned int use_viterbi;

	/* Use randomizer/scrambler */
	unsigned int use_randomizer;

	/* Use Reed Solomon error correction coding */
	unsigned int use_rs;

	/* Number of dummy bits generated to the tail */
	unsigned int tail_length;

	/* Additional flags in the golay code */
	unsigned int golay_flags;

};

/* Module descriptor */
extern const struct encoder_code golay_framer_code;

#endif /* __SUO_FRAMING_GOLAY_FRAMER_H__ */
