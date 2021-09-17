

#include "framing/utils.h"


size_t bytes_to_bits(bit_t *bits, size_t nbits, const uint8_t *bytes, bool lsb_first)
{
	size_t i;
	for (i = 0; i < nbits; i++) {
		const size_t bytenum = i >> 3, bitnum = i & 7;
		if (lsb_first)
			bits[i] = (bytes[bytenum] & (1 << bitnum)) ? 1 : 0;
		else
			bits[i] = (bytes[bytenum] & (0x80 >> bitnum)) ? 1 : 0;
	}
	return nbits;
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
