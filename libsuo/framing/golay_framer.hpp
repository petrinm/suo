#pragma once

#include "suo.hpp"
#include "coding/reed_solomon.hpp"
#include "coding/viterbi_decoder.hpp"


namespace suo {

/*
 */
class GolayFramer : public Block
{
public:

	enum State {
		Waiting = 0
	};

	struct Config {
		Config();

		/* Syncword */
		unsigned int sync_word;

		/* Number of bits in syncword */
		unsigned int sync_len;

		/* Number of bit */
		unsigned int preamble_len;

		/* Apply convolutional coding */
		unsigned int use_viterbi;

		/* Apply CCSDS randomizer/scrambler */
		unsigned int use_randomizer;

		/* Apply Reed Solomon error correction coding */
		unsigned int use_rs;

		/* Additional flags in the golay code */
		unsigned int golay_flags;

	};

	explicit GolayFramer(const Config& args = Config());

	void reset();

	void sourceSymbols(SymbolVector& symbols, Timestamp t);

	Port<Frame&, Timestamp> sourceFrame;

private:
	/* Configuration */
	Config conf;

	/* State */
	enum State state;
	int bit_idx;

	/* Buffers */
	ByteVector data_buffer;
	ByteVector viterbi_buffer;
	SampleVector bit_buffer;
	Frame frame;

};

}; // namespace suo
