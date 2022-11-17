#pragma once

#include "base_types.hpp"

namespace suo {

size_t bytes_to_bits(Bit *bits, const uint8_t *bytes, size_t nbytes, bool lsb_first);
size_t word_to_lsb_bits(Bit *bits, size_t nbits, uint64_t word);
size_t word_to_msb_bits(Bit *bits, size_t nbits, uint64_t word);

};
