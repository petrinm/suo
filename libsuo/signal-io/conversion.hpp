#pragma once

#include "base_types.hpp"

namespace suo {

static inline size_t cs16_to_cf(cs16_t *in, Sample *out, size_t n)
{
	size_t i;
	const float scale = 1.0f / 0x8000;
	for (i = 0; i < n; i++)
		out[i] = Sample((float)in[i][0] * scale, (float)in[i][1] * scale);
	return n;
}


static inline size_t cu8_to_cf(cu8_t *in, Sample *out, size_t n)
{
	size_t i;
	const float dc = -127.4f;
	const float scale = 1.0f / 127.6f;
	for (i = 0; i < n; i++)
		out[i] = Sample(((float)in[i][0] + dc) * scale, ((float)in[i][1] + dc) * scale);
	return n;
}


static inline size_t cf_to_cs16(Sample *in, cs16_t *out, size_t n)
{
	size_t i;
	const float scale = 0x8000;
	// TODO: clip
	for (i = 0; i < n; i++) {
		out[i][0] = in[i].real() * scale;
		out[i][1] = in[i].imag() * scale;
	}
	return n;
}

};
