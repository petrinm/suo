#include <iostream>

#include "framing/hdlc_framer.hpp"
#include "framing/hdlc_deframer.hpp"
#include "framing/utils.hpp"
#include "registry.hpp"



using namespace suo;
using namespace std;


const uint8_t START_FLAG = 0x7E;


HDLCFramer::Config::Config() {
	mode = G3RUH;
	preamble_length = 4;
	trailer_length = 4;
}

HDLCFramer::HDLCFramer(const Config& conf) :
	conf(conf)
{
	reset();
	reset_scrambler();
}


void HDLCFramer::reset_scrambler() {
	last_bit = 0;
	scrambler = 0;
	stuffing_counter = 0;
}

void HDLCFramer::reset() {
	//if (symbol_gen.running()) 
	//	symbol_gen.cancel();
	reset_scrambler();
	frame.clear();
}


Symbol HDLCFramer::scramble_bit(Symbol bit) {

	if (conf.mode == G3RUH) {
		
		// NRZ-I encoding
		if (bit == 1) { // If one, state remains
			bit = last_bit;
		}
		else { // If zero, state flips
			bit = !last_bit;
			last_bit = bit;
		}

		// G3RUH scrambling
		unsigned int lfsr_hi = (scrambler & 1);
		unsigned int lfsr_lo = ((scrambler >> 5) & 1);
		unsigned int scr_bit = (bit ^ (lfsr_hi ^ lfsr_lo)) != 0;

		scrambler = (scrambler >> 1);

		if (scr_bit)
			scrambler |= 0x00010000;

		return scr_bit;
	}
	else if (conf.mode == NRZI) {
		// NRZ-I encoding
		if (bit == 1) { // If one, state remains
			bit = last_bit;
		}
		else { // If zero, state flips
			bit = !last_bit;
			last_bit = bit;
		}
		return bit;
	}
	else
		return bit;
}


SymbolGenerator HDLCFramer::generateSymbols(Timestamp now) {
	sourceFrame.emit(frame, now);
	if (frame.empty() == false)
		return symbolGenerator();
	return SymbolGenerator();
}


SymbolGenerator HDLCFramer::symbolGenerator() // Frame& frame
{

	/* Append CRC to end of frame */
	if (conf.append_crc) {
		uint16_t crc = crc16_ccitt(&frame.data[0], frame.size());
		frame.data.push_back((crc >> 8) & 0xff);
		frame.data.push_back((crc >> 0) & 0xff);
	}

	reset_scrambler();

	/* Generate preamble symbols/bits */
	for (unsigned int i = 0; i < conf.preamble_length; i++) {
		Byte byte = START_FLAG;
		for (size_t bi = 8; bi > 0; bi--) {
			Symbol bit = (byte & 0x80) != 0;
			byte <<= 1;
			co_yield scramble_bit(bit);
		}
	}

	/* Generate symbols from the frame data */
	for (Byte byte: frame.data)
	{
		// Foreach bit
		for (size_t bi = 8; bi > 0; bi--)
		{
			Symbol bit = (byte & 0x80) != 0;
			byte <<= 1;

			if (stuffing_counter >= 5) {
				stuffing_counter = 0;
				co_yield scramble_bit(0);
			}
			stuffing_counter = bit ? (stuffing_counter + 1) : 0;

			co_yield scramble_bit(bit);
		}
	}


	/* Generate trailer symbols/bits */
	for (unsigned int i = 0; i < conf.trailer_length; i++) {
		Byte byte = START_FLAG;
		for (size_t bi = 8; bi > 0; bi--) {
			Symbol bit = (byte & 0x80) != 0;
			byte <<= 1;
			co_yield scramble_bit(bit);
		}
	}
	
	frame.clear();
}


Block* createHDLCFramer(const Kwargs& args)
{
	return new HDLCFramer();
}

static Registry registerHDLCFramer("HDLCFramer", &createHDLCFramer);
