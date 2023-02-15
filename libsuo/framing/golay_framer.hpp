#pragma once

#include "suo.hpp"
#include "coding/reed_solomon.hpp"
#include "coding/convolutional_encoder.hpp"


namespace suo {

/*
 */
class GolayFramer : public Block
{
public:

	struct Config {
		Config();

		/* Syncword */
		unsigned int syncword;

		/* Number of bits in syncword */
		unsigned int syncword_len;

		/* Number of bit */
		unsigned int preamble_len;

		/* Apply convolutional coding */
		unsigned int use_viterbi;

		/* Apply CCSDS randomizer/scrambler */
		unsigned int use_randomizer;

		/* Apply Reed Solomon error correction coding */
		unsigned int use_rs;

	};

	/*
	 */
	explicit GolayFramer(const Config& args = Config());

	GolayFramer(const GolayFramer&) = delete;
	GolayFramer& operator=(const GolayFramer&) = delete;


	/*
	 */
	void reset();

	/*
	 */
	void sourceSymbols(SymbolVector& symbols, Timestamp now);

	/*
	 */
	SymbolGenerator generateSymbols(Frame& frame);

	/*
	 */
	Port<Frame&, Timestamp> sourceFrame;

private:
	/* Configuration */
	Config conf;
	ReedSolomon rs;
	ConvolutionalEncoder conv_coder;

	/* Framer state */
	SymbolGenerator symbol_gen;
	Frame frame;

};

}; // namespace suo
