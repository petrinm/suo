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




static const uint8_t ParityTable[256] = {
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
};


unsigned int suo::bit_parity(uint8_t x) {
	return ParityTable[x];
}

unsigned int suo::bit_parity(uint16_t x) {
	x ^= (x >> 8);
	return ParityTable[x & 0xff];
}

unsigned int suo::bit_parity(uint32_t x) {
	x ^= (x >> 16);
	x ^= (x >> 8);
	return ParityTable[x & 0xff];
}

unsigned int suo::bit_parity(uint64_t x) {
	x ^= (x >> 32);
	x ^= (x >> 16);
	x ^= (x >> 8);
	return ParityTable[x & 0xff];
}




// O(1) bit reversal functions
// https://www.geeksforgeeks.org/reverse-bits-using-lookup-table-in-o1-time/

#define R2(n)  n, n + 2 * 64, n + 1 * 64, n + 3 * 64
#define R4(n)  R2(n), R2(n + 2 * 16), R2(n + 1 * 16), R2(n + 3 * 16)
#define R6(n)  R4(n), R4(n + 2 * 4), R4(n + 1 * 4), R4(n + 3 * 4)

static uint8_t reverse_uint8_table[256] = { R6(0), R6(2), R6(1), R6(3) };


uint8_t suo::reverse_bits(uint8_t num)
{
	return reverse_uint8_table[num];
}

uint16_t suo::reverse_bits(uint16_t num)
{
	return ((uint16_t)reverse_uint8_table[(num >> 0) & 0xff] << 8) |
	       ((uint16_t)reverse_uint8_table[(num >> 8) & 0xff] << 0);
}

uint32_t suo::reverse_bits(uint32_t num)
{
	return ((uint32_t)reverse_uint8_table[(num >>  0) & 0xff] << 24) |
	       ((uint32_t)reverse_uint8_table[(num >>  8) & 0xff] << 16) |
	       ((uint32_t)reverse_uint8_table[(num >> 16) & 0xff] <<  8) |
	       ((uint32_t)reverse_uint8_table[(num >> 24) & 0xff] <<  0);
}

uint64_t suo::reverse_bits(uint64_t num)
{
	return ((uint64_t)reverse_uint8_table[(num >>  0) & 0xff] << 56) |
	       ((uint64_t)reverse_uint8_table[(num >>  8) & 0xff] << 48) |
	       ((uint64_t)reverse_uint8_table[(num >> 16) & 0xff] << 40) |
	       ((uint64_t)reverse_uint8_table[(num >> 24) & 0xff] << 32) |
	       ((uint64_t)reverse_uint8_table[(num >> 32) & 0xff] << 24) |
	       ((uint64_t)reverse_uint8_table[(num >> 40) & 0xff] << 16) |
	       ((uint64_t)reverse_uint8_table[(num >> 48) & 0xff] <<  8) |
	       ((uint64_t)reverse_uint8_table[(num >> 56) & 0xff] <<  0);
}
