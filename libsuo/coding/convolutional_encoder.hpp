#pragma once

#include "suo.hpp"
#include "generators.hpp"

namespace suo
{


/*
 * Convolutional encoder configuration struct.
 */
struct ConvolutionalConfig {
	void validate() const;

	/* Constrain length */
	unsigned int k;

	/* Generator output rate without punturing */
	unsigned int rate;

	/* Generator polynomies */
	std::vector<int> polys;

	/* Puncturing pattern */
	std::vector<std::vector<unsigned int>> puncturing;
};


/*
 * Convolutional encoder
 */
class ConvolutionalEncoder
{
public:

	/*
	 */
	explicit ConvolutionalEncoder(const ConvolutionalConfig& conf);

	/*
	 */
	void reset(uint32_t start_state=0);

	/*
	 */
	double real_rate() const;

	void sourceSymbols(SymbolVector& symbols, Timestamp now);

	Port<SymbolVector&, Timestamp> sourceUncodedSymbols;

private:


	/*
	 * Generator for non-punctured convolutional codings
	 */
	SymbolGenerator generateSymbols(SymbolGenerator& gen);

	/*
	 * Generator for punctured convolutional coding
	 */
	SymbolGenerator generatePuncturedSymbols(SymbolGenerator& gen);

	/* Config */
	const ConvolutionalConfig& conf;

	/* State */
	unsigned int puncturing_index;
	uint32_t shift_register;
	SymbolVector input;
	SymbolVector encoderOutput;
	SymbolGenerator output_gen;
};

namespace ConvolutionCodes {

extern const ConvolutionalConfig AX5043;
extern const ConvolutionalConfig TI_CC11xx;
extern const ConvolutionalConfig CCSDS_1_2_7;
extern const ConvolutionalConfig CCSDS_2_3_7;
extern const ConvolutionalConfig CCSDS_3_4_7;
extern const ConvolutionalConfig CCSDS_4_5_7;
extern const ConvolutionalConfig CCSDS_5_6_7;
extern const ConvolutionalConfig CCSDS_1_3_7;

const ConvolutionalConfig& getConfig(const std::string_view name);

}; // namespace ConvolutionCodes

}; // namespace suo
