/*
 * GFSK demodulator implementation using correlator bank
 */
#ifndef DEMOD_GFSK_CORRBANK_H
#define DEMOD_GFSK_CORRBANK_H

#include "suo.h"

struct fsk_demod_corrbank_conf {
	/* Samples per symbol */
	unsigned sps;

	unsigned corr_len, corr_num;

	struct deframer_code deframer;
};


/* Module descriptor */
extern const struct receiver_code demod_fsk_corrbank;


#endif /* DEMOD_GFSK_CORRBANK_H */
