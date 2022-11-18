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
}

void HDLCFramer::reset() {
	state = GeneratePreamble;
	frame.clear();
}


Symbol HDLCFramer::scramble_bit(Symbol bit) {

	if (conf.mode == 0) { // NRZ-I
		if (bit == 1) { // If one, state remains
			bit = last_bit;
			last_bit = bit;
		}
		else { // If zero, state flips
			bit = !last_bit;
			last_bit = bit;
		}
	}

	// Scrambling
	unsigned int lfsr_hi = (scrambler & 1);
	unsigned int lfsr_lo = ((scrambler >> 5) & 1);
	unsigned int scr_bit = bit ^ (lfsr_hi ^ lfsr_lo);

	scrambler = (scrambler >> 1);

	if (scr_bit)
		scrambler |= 0x00010000;

	return scr_bit;
}



void HDLCFramer::sourceSymbols(SymbolVector& symbols, Timestamp now)
{
	if (frame.empty() == true) {
		// Get a frame
		sourceFrame.emit(frame, 0);
		if (frame.empty())
			return;

		if (!symbols.empty())
			throw SuoError("HDLCFramer: Symbol buffer not empty ");

		if (conf.append_crc) {
			// Append CRC to end of frame
			const size_t len = frame.size();
			uint16_t crc = crc16_ccitt(&frame.data[0], len);
			frame.data[len] = (crc >> 8) & 0xff;
			frame.data[len+1] = (crc >> 8) & 0xff;
			frame.data.resize(len + 2);
		}

		state = GeneratePreamble;
		last_bit = 0;
		scrambler = 0;
		stuffing_counter = 0;
		byte_idx = 0;
	}

	size_t max_symbols = symbols.capacity() - symbols.size();
	Symbol* bit_ptr = &static_cast<Symbol*>(symbols.data())[symbols.size()];

	if (state == GeneratePreamble) {
		symbols.flags |= start_of_burst;
		
		/* Make sure there's enough space in the symbol buffer */
		if (max_symbols < 8 * conf.preamble_length) {
			if (stuffing_counter++ > 8)
				throw SuoError("HDLCFramer: Too small symbol buffer");
			return;
		}

		/* Generate preamble symbols/bits */			
		for (unsigned int i = 0; i < conf.preamble_length; i++) {
			uint8_t byte = START_FLAG;
			for (size_t i = 8; i > 0; i--) {
				*bit_ptr++ = scramble_bit(byte & 0x80);
				byte <<= 1;
			}
		}

		max_symbols -= 8 * conf.preamble_length;
		state = GenerateData;
	}

	if (state == GenerateData) {

		// TODO: Proper indexing with partial symbol generation

		/* Generate symbols from the frame data */
		for (size_t i = 0; i < frame.size(); i++)
		{
			uint8_t byte = frame.data[i];
			for (size_t i = 8; i > 0; i--) // Foreach bit
			{
				Symbol bit = (byte & 0x80) != 0;
				byte <<= 1;

				if (bit == 1) { 
					if (stuffing_counter > 5) {
						*bit_ptr++ = scramble_bit(0);
						stuffing_counter = 0;
					}
					else 
						stuffing_counter++;
				}

				*bit_ptr++ = scramble_bit(bit);
			}
		}
		
		max_symbols -= 8 * conf.preamble_length;
		stuffing_counter = 0;
		state = GenerateData;
	}

	if (state == GenerateTrailer) {

		/* Make sure there's enough space in the symbol buffer */
		if (max_symbols < 8 * conf.trailer_length) {
			if (stuffing_counter++ > 8)
				throw SuoError("HDLCFramer: Too small symbol buffer");
			return;
		}

		/* Generate trailer symbols/bits */
		for (unsigned int i = 0; i < conf.trailer_length; i++) {
			uint8_t byte = START_FLAG;
			for (size_t i = 8; i > 0; i--) {
				*bit_ptr++ = scramble_bit(byte & 0x80);
				byte <<= 1;
			}
		}

		reset();
	}

}


Block* createHDLCFramer(const Kwargs& args)
{
	return new HDLCFramer();
}

static Registry registerHDLCFramer("HDLCFramer", &createHDLCFramer);
