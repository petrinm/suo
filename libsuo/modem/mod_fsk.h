/*
 * (G)FSK modulator
 */

#ifndef __LIBSUO_MOD_FSK_H__
#define __LIBSUO_MOD_FSK_H__

#include "suo.h"

struct mod_fsk_conf {

	/* Output sample rate as samples per second*/
	float samplerate;

	/* Symbol rate as symbols per second */
	float symbolrate;

	/* Number of bit in one symbol (symbol complexity) */
	unsigned bits_per_symbol;

	/* Center frequency */
	float centerfreq;

	/* Modulation index. 0.5 for MSK */
	float modindex;

	/* Gaussian filter bandwidth-symbol time product */
	float bt;
};

/* Modulator descriptor */
extern const struct transmitter_code mod_fsk_code;

#endif /* __LIBSUO_MOD_FSK_H__ */
