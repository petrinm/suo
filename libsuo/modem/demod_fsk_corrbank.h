/*
 * GFSK/GMSK demodulator implementation using correlator bank
 */
#ifndef __LIBSUO_DEMOD_GFSK_CORRBANK_H__
#define __LIBSUO_DEMOD_GFSK_CORRBANK_H__

#include "suo.h"

struct fsk_demod_corrbank_conf {

	/* Samples per symbol */
	unsigned int sps;

	/* Correlator lenght */
	unsigned int corr_len;

	/* Number of correlators */
	unsigned int corr_num;

};


/* Module descriptor */
extern const struct receiver_code demod_fsk_corrbank_code;


#endif /* __LIBSUO_DEMOD_GFSK_CORRBANK_H__ */
