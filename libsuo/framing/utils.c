

#include "framing/utils.h"


size_t bytes_to_bits(bit_t *bits, const uint8_t *bytes, size_t nbytes, bool lsb_first)
{
#if 0
	size_t i;
	for (i = 0; i < nbits; i++) {
		const size_t bytenum = i >> 3, bitnum = i & 7;
		if (lsb_first)
			bits[i] = (bytes[bytenum] & (1 << bitnum)) ? 1 : 0;
		else
			bits[i] = (bytes[bytenum] & (0x80 >> bitnum)) ? 1 : 0;
	}
	return nbits;
#endif

	if (lsb_first == 1) {
		for (size_t i = 0; i < nbytes; i++)
			bits += word_to_lsb_bits(bits, 8, *(bytes++));
	}
	else {
		for (size_t i = 0; i < nbytes; i++)
			bits += word_to_msb_bits(bits, 8, *(bytes++));
	}

	return 8 * nbytes;
}


size_t word_to_lsb_bits(bit_t *bits, size_t nbits, uint64_t word)
{
	register size_t i;
	for (i = nbits; i-- > 0; ) {
		bits[i] = word & 1;
		word >>= 1;
	}
	return nbits;
}

size_t word_to_msb_bits(bit_t *bits, size_t nbits, uint64_t word)
{
	register size_t i;
	for (i = nbits; i-- > 0; ) {
		bits[i] = word & 1;
		word >>= 1;
	}
	return nbits;
}
