#pragma once

#include "suo.hpp"

namespace suo
{


class ReedSolomonUncorrectable: public SuoError {
public:
	ReedSolomonUncorrectable(const char* msg) : SuoError(msg) {}
};

/*
 * Generate conversion lookup tables between conventional alpha representation
 *	(@**7, @**6, ...@**0)
 * and Berlekamp's dual basis representation
 *	(l0, l1, ...l7)
 *
 * References:
 * - CCSDS: TM Synchronization and Channel Coding (131.0-B-4)
 *   Section 4.3.9: Dual Basis Representation
 *   and Annex F: Transformation between Berlekamp and Conventional Representations
 *
 * - Phil Karman - LibFEC
 *   https://github.com/quiet/libfec/blob/master/gen_ccsds_tal.c
 */
void generateDualBasisLookupTable();

/*
 * Convert data bytes from alpha representation to dual basis representation.
 */
void convertToDualBasis(ByteVector& data);

/*
 * Convert data bytes from dual basis representation to alpha representation.
 */
void convertFromDualBasis(ByteVector& data);


/* */
struct ReedSolomonConfig {

	/* Number of bits in a symbol */
	unsigned int symbol_size;

	/* Generator primitive polynomial to generate polynomial roots */
	uint16_t primitive_polynomial;

	/* First consecutive root (FCR) in index form */
	uint8_t first_consecutive_root;

	/* Generator root gap */
	uint8_t generator_root_gap;

	/* Number of coded bytes */
	unsigned int coded_bytes;

	//RS code generator polynomial degree (number of roots)
	/* Number of generated roots. Also number of parity bytes. */
	unsigned int num_roots;

	/* */
	unsigned int pad;
};


/* */
class ReedSolomon
{
public:
	using DataType = uint8_t;

	explicit ReedSolomon(const ReedSolomonConfig& cfg);

	//~ReedSolomon();

	/* */
	void encode(std::vector<DataType>& msg) const;

	/* 
	 * Decode 
	 * Returns number of corrected roots. */
	unsigned int decode(std::vector<DataType>& msg) const;
	unsigned int decode(std::vector<DataType>& msg, std::vector<unsigned int>& erasures) const;

	/* */
	void deinterleave(ByteVector& data, ByteVector& output, uint8_t pos, uint8_t i);

	/* */
	void interleave(ByteVector& data, ByteVector& output, uint8_t pos, uint8_t i);

private:

	/* Modulo operation in galoys fields */
	unsigned int modnn(unsigned int x) const;

	bool dual_basis;
	const ReedSolomonConfig& cfg;

	std::vector<DataType> genpoly;

	std::vector<uint8_t> alpha_to;
	std::vector<uint8_t> index_of;
	std::vector<uint8_t> poly;

	unsigned int symbol_count;

	int iprim; // prim-th root of 1, index form 

};


unsigned int count_bit_errors(const ByteVector& a, const ByteVector& b);

namespace RSCodes {

extern const ReedSolomonConfig CCSDS_RS_255_223;
extern const ReedSolomonConfig CCSDS_RS_255_239;

/* Get Reed Solomon configuration by name */
const ReedSolomonConfig& getConfig(const std::string_view name);

};

}; // namespace suo
