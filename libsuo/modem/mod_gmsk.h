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

	/* Center frequency (Hz) */
	float centerfreq;

	/* Gaussian filter bandwidth-symbol time product */
	float bt;

	/* Length of the start/stop ramp in samples */
	unsigned int ramp;
};

/* Modulator descriptor */
extern const struct transmitter_code mod_gmsk_code;

#endif
