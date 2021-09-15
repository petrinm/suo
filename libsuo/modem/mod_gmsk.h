/*
 * GMSK modulator
 */

#ifndef LIBSUO_MOD_GMSK_H
#define LIBSUO_MOD_GMSK_H

#include "suo.h"

struct mod_gmsk_conf {

	/* Output sample rate as samples per second */
	float samplerate;

	/* Symbol rate as symbols per second */
	float symbolrate;

	/* Center frequency */
	float centerfreq;

	/* Gaussian filter bandwidth-symbol time product */
	float bt;
};

/* Modulator descriptor */
extern const struct transmitter_code mod_gmsk_code;

#endif
