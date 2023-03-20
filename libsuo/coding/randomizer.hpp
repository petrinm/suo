#pragma once

#include "suo.hpp"
#include <cstdint>

namespace suo {

const size_t ccsds_randomizer_len = 256;
extern const uint8_t ccsds_tm_randomizer[ccsds_randomizer_len];
extern const uint8_t ccsds_tc_randomizer[ccsds_randomizer_len];

const size_t pn9_randomizer_len = 511;
extern const uint8_t pn9_randomizer[pn9_randomizer_len];

}; // namespace suo 
