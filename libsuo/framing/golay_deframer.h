#ifndef __SUO_FRAMING_GOLAY_DEFRAMER_H__
#define __SUO_FRAMING_GOLAY_DEFRAMER_H__

#include "suo.h"

struct golay_deframer_conf {

	/* Syncword */
	unsigned int sync_word;

	/* Number of bits in syncword */
	unsigned int sync_len;

	/* Maximum number of bit errors */
	unsigned int sync_threshold;

	/* Skip Reed-solomon coding */
	unsigned int skip_rs;

	/* Skip randomizer/scrambler */
	unsigned int skip_randomizer;

	/* Skip viterbi convolutional code */
	unsigned int skip_viterbi;
};

/* Module descriptor */
extern const struct decoder_code golay_deframer_code;

#endif /* __SUO_FRAMING_GOLAY_DEFRAMER_H__ */
