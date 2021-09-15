/*
 * (G)FSK demodulator implementation using matched filters.
 */

#ifndef LIBSUO_DEMOD_FSK_MFILT_H
#define LIBSUO_DEMOD_FSK_MFILT_H

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

	/* Number of bit in one symbol (symbol complexity) */
	//unsigned bits_per_symbol;

	/*
	 * Signal center frequency as Hz
	 */
	float centerfreq;

	/*
	 * Sync word which is being searched
	 */
	uint64_t syncword;

	/*
	 * Sync word length (symbols)
	 */
	unsigned int synclen;

	unsigned int framelen;
};

/* Module descriptor */
extern const struct receiver_code demod_fsk_mfilt;

#endif /* LIBSUO_DEMOD_FSK_MFILT_H */
