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
	legacy_mode = false;
}


GolayFramer::GolayFramer(const Config& conf) :
	conf(conf),
	rs(RSCodes::CCSDS_RS_255_223),
	conv_encoder(ConvolutionCodes::CCSDS_1_2_7)
{
	if (conf.syncword_len > 8 * sizeof(conf.syncword))
		throw SuoError("Unrealistic syncword length");

	if (conf.preamble_len > 1024)
		throw SuoError("Unrealistic preamble length");

	reset();
}


void GolayFramer::reset() {
	//if (symbol_gen.running()) 
	//	symbol_gen.cancel();
	frame.clear();
}


SymbolGenerator GolayFramer::generateSymbols(Timestamp now) {
	frame.clear();
	sourceFrame.emit(frame, now);
	if (frame.empty() == false)
		return symbolGenerator(frame);
	return SymbolGenerator();
}


SymbolGenerator GolayFramer::symbolGenerator(Frame& frame)
{
	/* Generate preamble sequence */
	for (size_t i = 0; i < conf.preamble_len; i++)
		co_yield (i & 1);

	/* Append syncword */
	co_yield word_to_lsb_bits(conf.syncword, conf.syncword_len);


	/* Calculate Reed Solomon */
	ByteVector data_buffer = frame.data;
	if (conf.use_rs)
		rs.encode(data_buffer);

	/* Scrambler the bytes */
	if (conf.use_randomizer) {
		for (size_t i = 0; i < data_buffer.size(); i++)
			data_buffer[i] ^= ccsds_randomizer[i];
	}

	/* Output Golay coded length (+coding flags) */
	uint32_t coded_len = data_buffer.size();
	if (conf.legacy_mode) {
		if (conf.use_rs) coded_len |= GolayFramer::use_reed_solomon_flag;
		if (conf.use_randomizer) coded_len |= GolayFramer::use_randomizer_flag;
		if (conf.use_viterbi) coded_len |= GolayFramer::use_viterbi_flag;
	}

	encode_golay24(&coded_len);
	co_yield word_to_lsb_bits(coded_len, 24);


	/* Viterbi decode all bits */
	if (conf.use_viterbi) {
		SymbolVector viterbi_buffer;
		viterbi_buffer.reserve(2 * frame.size());
		//conv_encoder.sourceSymbols(viterbi_buffer);
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
