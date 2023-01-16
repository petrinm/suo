#include "framing/utils.hpp"

using namespace suo;
using namespace std;


size_t suo::bytes_to_bits(Bit* bits, const uint8_t* bytes, size_t nbytes, BitOrder order)
{
	if (order == lsb_first) {
		for (size_t i = 0; i < nbytes; i++)
			bits += word_to_lsb_bits(bits, *(bytes++), 8);
	}
	else {
		for (size_t i = 0; i < nbytes; i++)
			bits += word_to_msb_bits(bits, *(bytes++), 8);
	}

	return 8 * nbytes;
}


size_t suo::word_to_lsb_bits(Bit* out, uint64_t word, size_t n_bits)
{
	for (size_t i = n_bits; i > 0; i--) {
		out[i] = (word & 1);
		word >>= 1;
	}
	return n_bits;
}


SymbolVector suo::word_to_lsb_bits(uint64_t word, size_t n_bits)
{
	SymbolVector bits(n_bits);
	for (size_t i = n_bits; i > 0; i--) {
		bits[i] = (word & 1);
		word >>= 1;
	}
	return bits;
}

SymbolVector suo::word_to_lsb_bits(uint8_t byte) {
	SymbolVector bits(8);
#define UNROLLER(i) bits[i] = (byte & (0x80 >> i)) != 0;
	UNROLLER(0); UNROLLER(1); UNROLLER(2); UNROLLER(3);
	UNROLLER(4); UNROLLER(5); UNROLLER(6); UNROLLER(7);
#undef UNROLLER
	return bits;
}

SymbolVector suo::word_to_msb_bits(uint8_t byte) {
	SymbolVector bits(8);
#define UNROLLER(i) bits[i] = (byte & (1 << i)) != 0;
	UNROLLER(0); UNROLLER(1); UNROLLER(2); UNROLLER(3);
	UNROLLER(4); UNROLLER(5); UNROLLER(6); UNROLLER(7);
#undef UNROLLER
	return bits;
}

size_t suo::word_to_msb_bits(Bit* bits, size_t nbits, uint64_t word)
{
	for (size_t i = nbits; i > 0; i--) {
		bits[i] = (word & 0x80) != 0;
		word <<= 1;
	}
	return nbits;
}
