/*
 * Syncword deframer
 * Support fixed length frames or frames with length byte
 */
#ifndef __SUO_FRAMING_SYNCWORD_FRAMER_H__
#define __SUO_FRAMING_SYNCWORD_FRAMER_H__

#include "suo.h"

struct syncword_framer_conf {

	/* Syncword */
	unsigned int sync_word;

	/* Number of bits in syncword */
	unsigned int sync_len;

	/* Fixed frame length. If 0 uses length byte. */
	unsigned int fixed_length;

};

extern const struct encoder_code syncword_framer;

#endif /* __LIBSUO_SYNCWORD_FRAMER_H__ */
