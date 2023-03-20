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

	/* Header flags for Gomspace's legacy U482C mode */
	static const unsigned int use_reed_solomon_flag = 0x200;
	static const unsigned int use_randomizer_flag = 0x400;
	static const unsigned int use_viterbi_flag = 0x800;

	struct Config {
		Config();

		/* Syncword */
		unsigned int syncword;

		/* Number of bits in syncword */
		unsigned int syncword_len;

		/* Number of bit */
		unsigned int preamble_len;

		/* Apply convolutional coding */
		bool use_viterbi;

		/* Apply CCSDS randomizer/scrambler */
		bool use_randomizer;

		/* Apply Reed Solomon error correction coding */
		bool use_rs;

		/* Legacy mode for GommSpace's U482C radios */
		bool legacy_mode;
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
	SymbolGenerator generateSymbols(Timestamp now);

	/*
	 */
	Port<Frame&, Timestamp> sourceFrame;

private:
	
	SymbolGenerator symbolGenerator(Frame& frame);

	/* Configuration */
	Config conf;
	ReedSolomon rs;
	ConvolutionalEncoder conv_encoder;

	/* Framer state */
	SymbolGenerator symbol_gen;
	Frame frame;

};

}; // namespace suo
