#include <iostream>
#include <cstring>

#include "registry.hpp"
#include "framing/golay_framer.hpp"
#include "framing/utils.hpp"
#include "coding/golay24.hpp"
#include "coding/randomizer.hpp"


using namespace std;
using namespace suo;


#define MAX_FRAME_LENGTH  255
#define RS_LEN      32


GolayFramer::Config::Config() {
	sync_word = 0xC9D08A7B;
	sync_len = 32;
	preamble_len = 0x50;
	use_viterbi = 0;
	use_randomizer = 1;
	use_rs = 1;
	golay_flags = 0;
}


GolayFramer::GolayFramer(const Config& conf) :
	conf(conf)
{
	data_buffer.reserve(MAX_FRAME_LENGTH);
	reset();
}

void GolayFramer::reset() {
	state = Waiting;
	frame.clear();
}

void GolayFramer::sourceSymbols(SymbolVector& symbols, Timestamp t)
{
	if (frame.empty()) {
		// Get a frame
		sourceFrame.emit(frame, 0);
		if (frame.empty())
			return;
	}

	const size_t max_symbols = symbols.capacity();

	/* Start processing a frame */
	unsigned int payload_len = frame.data.size();
	if (conf.use_rs)
	 	payload_len += RS_LEN;

	// TODO: Allow symbol generation in smaller partitions
	size_t syms = conf.preamble_len + conf.sync_len + 24 + 8 * payload_len;
	if (max_symbols < syms)
		throw SuoError("Too small buffer! Symbols to generate %d, buffer size %d", syms, max_symbols);

	symbols.resize(syms);
	symbols.flags = start_of_burst | end_of_burst;

	/* Generate preamble sequence */
	Symbol *bit_ptr = symbols.data();
	for (unsigned int i = 0; i < conf.preamble_len; i++)
		*bit_ptr++ = i & 1;

	/* Append syncword */
	bit_ptr += word_to_lsb_bits(bit_ptr, conf.sync_len, conf.sync_word);

	/* Append Golay coded length (+coding flags) */
	uint32_t coded_len = conf.golay_flags | payload_len;
	if (conf.use_rs)
		coded_len |= 0x200;
	if (conf.use_randomizer)
		coded_len |= 0x400;
	if (conf.use_viterbi)
		coded_len |= 0x800;

	encode_golay24(&coded_len);
	bit_ptr += word_to_lsb_bits(bit_ptr, 24, coded_len);

	/* Calculate Reed Solomon */
	if (conf.use_rs) {
		//fec_encode(l_rs, frame->data_len, (unsigned char*)frame->data, data_buffer);
	}
	else {
		data_buffer = frame.data;
		//memcpy(data_buffer.data(), frame->raw_ptr(), frame->size());
	}

	/* Scrambler the bytes */
	if (conf.use_randomizer) {
		for (size_t i = 0; i < frame.data.size(); i++)
			data_buffer[i] ^= ccsds_randomizer[i];
	}

	/* Viterbi decode all bits */
	if (conf.use_viterbi) {
		//fec_encode(l_viterbi, frame->data_len, data_buffer, viterbi_buffer);
		//bit_ptr += bytes_to_bits(bit_ptr, viterbi_buffer, 2 * payload_len, 1);
	}
	else {
		bit_ptr += bytes_to_bits(bit_ptr, data_buffer.data(), payload_len, 1);
	}
		
	frame.clear();
}

Block* createGolayFramer(const Kwargs &args)
{
	return new GolayFramer();
}

static Registry registerGolayFramer("GolayFramer", &createGolayFramer);
