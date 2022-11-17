#pragma once

#include <memory>
#include "suo.hpp"

namespace suo
{

/*
 */
class HDLCFramer : public Block
{
public:

	enum State {
		GeneratePreamble,
		GenerateData,
		GenerateTrailer,
	};

	enum HDLCMode {
		Plain,
		G3RUH,
	};

	struct Config {
		Config();

		/* */
		HDLCMode mode;

		/* Number of preamble bytes */
		unsigned int preamble_length;

		/* Number of trailer bytes */
		unsigned int trailer_length;

		/* If true, CRC1-6 CCITT is appended to the end of the frame. */
		bool append_crc;

	};

	explicit HDLCFramer(const Config& conf = Config());
	
	void reset();

	void sourceSymbols(SymbolVector& symbols, Timestamp now);
	
	Port<Frame&, Timestamp> sourceFrame;

private:

	Symbol scramble_bit(Symbol bit);

	/* Configuration */
	Config conf;

	/* State */
	State state;
	size_t byte_idx;
	Symbol last_bit;
	unsigned int scrambler;
	unsigned int stuffing_counter;
	
	Frame frame;
};

}; // namespace suo
