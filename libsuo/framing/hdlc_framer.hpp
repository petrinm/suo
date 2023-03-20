#pragma once

#include <memory>
#include "suo.hpp"
#include "generators.hpp"
#include "framing/hdlc_deframer.hpp"

namespace suo
{

/*
 */
class HDLCFramer : public Block
{
public:
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
	
	HDLCFramer(const HDLCFramer&) = delete;
	HDLCFramer& operator=(const HDLCFramer&) = delete;

	/* */
	void reset();

	/* */
	SymbolGenerator generateSymbols(Timestamp now);

	/* */
	Port<Frame&, Timestamp> sourceFrame;

private:

	/* Coroutine for geneting symbol sequence */
	SymbolGenerator symbolGenerator();

	/* Bit scrambler */
	Symbol scramble_bit(Symbol bit);
	void reset_scrambler();

	/* Configuration */
	Config conf;

	/* State */
	Symbol last_bit;
	unsigned int scrambler;
	unsigned int stuffing_counter;
	SymbolGenerator symbol_gen;

	Frame frame;
};

}; // namespace suo
