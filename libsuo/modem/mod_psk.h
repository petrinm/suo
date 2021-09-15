/*
 * PSK demodulator
 */
#ifndef LIBSUO_MOD_PSK_H
#define LIBSUO_MOD_PSK_H

#include "suo.h"

/**/
struct mod_psk_conf {

	/**/
	float samplerate;

	/**/
	float symbolrate;

	/**/
	float centerfreq;
};

extern const struct transmitter_code mod_psk;

#endif /* LIBSUO_MOD_PSK_H */
