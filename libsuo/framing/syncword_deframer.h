/*
 * Syncword deframer
 * Support fixed length frames or frames with length byte
 */
#ifndef __SUO_FRAMING_SYNCWORD_DEFRAMER_H__
#define __SUO_FRAMING_SYNCWORD_DEFRAMER_H__

#include "suo.h"

struct syncword_deframer_conf {

	/* Syncword */
	unsigned int sync_word;

	/* Number of bits in syncword */
	unsigned int sync_len;

	/* Maximum number of bit errors */
	unsigned int sync_threshold;

	/* Fixed frame length. If 0 uses length byte. */
	unsigned int fixed_length;

};

extern const struct decoder_code syncword_deframer;

#endif /* __SUO_FRAMING_SYNCWORD_DEFRAMER_H__ */
