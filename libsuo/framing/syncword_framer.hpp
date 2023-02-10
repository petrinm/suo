#pragma once

#include "suo.hpp"

namespace suo {

/*
 * Syncword deframer
 * Support fixed length frames or frames with length byte
 */
class SyncwordFramer: public Block
{
public:
	struct Config {
		Config();

		/* Syncword */
		unsigned int syncword;

		/* Number of bits in syncword */
		unsigned int syncword_len;

		/* Fixed frame length. If 0 uses length byte. */
		unsigned int fixed_length;

		unsigned int preamble_len;

	};

	explicit SyncwordFramer(const Config& conf = Config());

	SyncwordFramer(const SyncwordFramer&) = delete;
	SyncwordFramer& operator=(const SyncwordFramer&) = delete;

	/*
	 */
	void reset();

	/*
	 */
	void sourceSymbols(SymbolVector& symbols, Timestamp t);

	/*
	 */
	SymbolGenerator generateSymbols(const Frame& frame);

	/*
	 */
	Port<Frame&, Timestamp> sourceFrame;

private:
	Config conf;

	/* Framer state */
	SymbolGenerator symbol_gen;
	Frame frame;

};

};
