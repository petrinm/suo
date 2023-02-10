#include <iostream>
#include <cstring>

#include "registry.hpp"
#include "framing/syncword_framer.hpp"
#include "framing/utils.hpp"


using namespace std;
using namespace suo;



SyncwordFramer::Config::Config() {
	syncword = 0xC9D08A7B;
	syncword_len = 32;
	preamble_len = 80;
}


SyncwordFramer::SyncwordFramer(const Config& conf):
	conf(conf)
{
	reset();
}


void SyncwordFramer::reset() {
	//if (symbol_gen.running()) 
	//	symbol_gen.cancel();
	frame.clear();
}


void SyncwordFramer::sourceSymbols(SymbolVector& symbols, Timestamp now) {

	if (symbol_gen.running()) {
		// Get a frame
		if (frame.empty()) {
		sourceFrame.emit(frame, now);
			symbol_gen = generateSymbols(frame);
			symbol_gen.sourceSymbols(symbols);
		}
	}
	else {
		symbol_gen.sourceSymbols(symbols);
	}

}

SymbolGenerator SyncwordFramer::generateSymbols(const Frame& frame)
{

	/* Generate preamble sequence */
	for (size_t i = 0; i < conf.preamble_len; i++)
		co_yield (i & 1);

	/* Append syncword */
	co_yield word_to_lsb_bits(conf.syncword, conf.syncword_len);

	/* If variable length */
	if (0) {
		unsigned int payload_len = frame.size();
		co_yield word_to_lsb_bits(payload_len, 8);
	}

	/* Feed the data bits */
	for (Byte byte : frame.data)
		co_yield word_to_lsb_bits(byte);

}

Block* createSyncwordFramer(const Kwargs& args)
{
	return new SyncwordFramer();
}

static Registry registerSyncwordFramer("SyncwordFramer", &createSyncwordFramer);
