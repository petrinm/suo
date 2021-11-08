/*
 *
 */

#ifndef LIBSUO_DEMOD_GMSK_H
#define LIBSUO_DEMOD_GMSK_H

#include "suo.h"

/* Configuration struct for the GMSK demod block */
struct demod_gmsk_conf {
	/*
	 * Input IQ sample rate as samples per second.
	 */
	float sample_rate;

	/*
	 * Symbol rate as symbols per second
	 */
	float symbol_rate;

	/*
	 * Signal center frequency as Hz
	 */
	float center_frequency;

	/* bandwidth product */
	float bt;

	unsigned int sync_word;
	unsigned int sync_len;

};

/* Global module descriptor */
extern const struct receiver_code demod_gmsk_code;

#endif /* LIBSUO_DEMOD_GMSK_H */
