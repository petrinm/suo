#pragma once

#include "suo.hpp"
#include "crc_algorithms.hpp"

#include <array>

namespace suo
{


/* */
class CRC
{
public:
	CRC(const CRCAlgorithm& algo);

	/* */
	uint32_t init() const;

	/* */
	uint32_t init(uint32_t initial) const;

	/* */
	uint32_t update(uint32_t crc, const ByteVector& bytes) const;

	/* */
	uint32_t finalize(uint32_t crc) const;

	/* */
	uint32_t calculate(const ByteVector& bytes) const;

private:
	const CRCAlgorithm& algo;
};


}; // namespace suo
