/*
 * (G)FSK demodulator implementation using matched filters.
 */

#ifndef __LIBSUO_DEMOD_FSK_MFILT_H__
#define __LIBSUO_DEMOD_FSK_MFILT_H__

#include "suo.h"

/* Configuration struct for the FSK demod */
struct fsk_demod_mfilt_conf {
	/*
	 * Input IQ sample rate as samples per second.
	 */
	float samplerate;

	/*
	 * Symbol rate as symbols per second
	 */
	float symbolrate;

	/*
	 * Number of bit in one symbol (symbol complexity)
	 */
	unsigned bits_per_symbol;

	/*
	 * Signal center frequency as Hz
	 */
	float center_freq;

};

/* Module descriptor */
extern const struct receiver_code demod_fsk_mfilt_code;

#endif /* __LIBSUO_DEMOD_FSK_MFILT_H__ */
