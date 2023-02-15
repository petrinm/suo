#include <iostream>
#include <cstring>

#include "registry.hpp"
#include "framing/golay_framer.hpp"
#include "framing/utils.hpp"
#include "coding/golay24.hpp"
#include "coding/randomizer.hpp"


using namespace std;
using namespace suo;



GolayFramer::Config::Config() {
	syncword = 0xC9D08A7B;
	syncword_len = 32;
	preamble_len = 80;
	use_viterbi = false;
	use_randomizer = true;
	use_rs = true;
}


GolayFramer::GolayFramer(const Config& conf) :
	conf(conf),
	rs(RSCodes::CCSDS_RS_255_223),
	conv_coder(ConvolutionCodes::CCSDS_1_2_7)
{

	if (conf.syncword_len > 8 * sizeof(conf.syncword))
		throw SuoError("Unrealistic syncword length");

	reset();
}


void GolayFramer::reset() {
	//if (symbol_gen.running()) 
	//	symbol_gen.cancel();
	frame.clear();
}


void GolayFramer::sourceSymbols(SymbolVector& symbols, Timestamp now)
{
	if (symbol_gen.running()) {
		cout << "more gen" << endl;
		// Source more symbols from generator
		symbol_gen.sourceSymbols(symbols);
	}
	else {
		// Try to source a new frame
		frame.clear();
		sourceFrame.emit(frame, now);
		if (frame.empty() == false) {
			cout << "new gen" << endl;
			symbol_gen = generateSymbols(frame);
			symbol_gen.sourceSymbols(symbols);
		}
	}
}

SymbolGenerator GolayFramer::generateSymbols(Frame& frame)
{
	/* Append Reed-Solomon FEC */
	if (conf.use_rs)
		rs.encode(frame.data);

	/* Generate preamble sequence */
	for (size_t i = 0; i < conf.preamble_len; i++)
		co_yield (i & 1);

	/* Append syncword */
	co_yield word_to_lsb_bits(conf.syncword, conf.syncword_len);

	/* Append Golay coded length (+coding flags) */
	uint32_t coded_len = frame.size();
	if (conf.use_rs) coded_len |= 0x200;
	if (conf.use_randomizer) coded_len |= 0x400;
	if (conf.use_viterbi) coded_len |= 0x800;

	encode_golay24(&coded_len);
	co_yield word_to_lsb_bits(coded_len, 24);

	/* Calculate Reed Solomon */
	ByteVector data_buffer = frame.data; // (conf.use_rs) ? rs.encode(frame.data) : frame.data;

	/* Scrambler the bytes */
	if (conf.use_randomizer) {
		for (size_t i = 0; i < frame.size(); i++)
			data_buffer[i] ^= ccsds_randomizer[i];
	}

	/* Viterbi decode all bits */
	if (conf.use_viterbi) {
		SymbolVector viterbi_buffer;
		viterbi_buffer.reserve(2 * frame.size());

		//fec_encode(frame->data_len, data_buffer, viterbi_buffer);
		//viterbi.sourceSymbols();
		co_yield viterbi_buffer;
	}
	else {
		for (Byte byte: data_buffer)
			co_yield word_to_lsb_bits(byte);
	}
	
}

Block* createGolayFramer(const Kwargs &args)
{
	return new GolayFramer();
}

static Registry registerGolayFramer("GolayFramer", &createGolayFramer);
