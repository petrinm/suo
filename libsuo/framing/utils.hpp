#pragma once

#include "base_types.hpp"
#include "vectors.hpp"

namespace suo {

enum BitOrder {
	msb_first,
	lsb_first
};


size_t bytes_to_bits(Bit* bits, const uint8_t* bytes, size_t nbytes, BitOrder order);

size_t word_to_lsb_bits(Bit* bits, uint64_t word, size_t nbits);
SymbolVector word_to_lsb_bits(uint64_t word, size_t n_bits);
SymbolVector word_to_lsb_bits(uint8_t byte);
SymbolVector word_to_msb_bits(uint8_t byte);

size_t word_to_msb_bits(Bit* bits, uint64_t word, size_t nbits);


/* 
 * Reverse the bit order of given value. 
 */
uint8_t reverse_bits(uint8_t val);
uint16_t reverse_bits(uint16_t val);
uint32_t reverse_bits(uint32_t val);
uint64_t reverse_bits(uint64_t val);


/*
 * Calculate the bit parity of the given value.
 */
unsigned int bit_parity(uint8_t val);
unsigned int bit_parity(uint16_t val);
unsigned int bit_parity(uint32_t val);
unsigned int bit_parity(uint64_t val);

};
