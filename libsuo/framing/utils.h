#ifndef __LIBSUO_FRAMING_UTILS_H__
#define __LIBSUO_FRAMING_UTILS_H__

#include "suo.h"

size_t bytes_to_bits(bit_t *bits, size_t nbits, const uint8_t *bytes, bool lsb_first);
size_t word_to_lsb_bits(bit_t *bits, size_t nbits, uint64_t word);
size_t word_to_msb_bits(bit_t *bits, size_t nbits, uint64_t word);

#endif /* __LIBSUO_FRAMING_UTILS_H__ */
