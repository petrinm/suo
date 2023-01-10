#pragma once

#include "suo.hpp"

#include <array>

namespace suo
{

/* CRC algorithm definition */
struct CRCAlgorithm {

	/* Number of bits in the linear feedback register. */
	unsigned int width;

	/* The generator polynomial that sets the feedback tap positions of the shift register. */
	uint32_t poly;

	/* Initial value of the shift register. */
	uint32_t init;

	/* Reflect input bit order. */
	bool refIn;

	/* Reflect output bit order. */
	bool refOut;

	/* XOR output */
	uint32_t xorOut;
};


namespace CRCAlgorithms {

extern const CRCAlgorithm CRC8;
extern const CRCAlgorithm CRC8_CDMA2000;
extern const CRCAlgorithm CRC8_DVB_S2;
extern const CRCAlgorithm CRC8_ITU;

extern const CRCAlgorithm CRC16_AUG_CCITT;
extern const CRCAlgorithm CRC16_CCITT_FALSE;
extern const CRCAlgorithm CRC16_CDMA2000;
extern const CRCAlgorithm CRC16_X25;

extern const CRCAlgorithm CRC24_BLE;

extern const CRCAlgorithm CRC32;
extern const CRCAlgorithm CRC32_POSIX;

};

/* Get CRC configuration by name */
const CRCAlgorithm& getCRC(const std::string_view name);

}; // namespace suo
