#include <iostream>

#include "framing/hdlc_deframer.hpp"
#include "registry.hpp"

using namespace std;
using namespace suo;


#define START_BYTE 0x7E


uint16_t suo::crc16_ccitt(const uint8_t* data_p, size_t length)
{
	uint16_t crc = 0xFFFF;
	const uint16_t crc16_table[] = {
		0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
		0x8408, 0x9489, 0xa50a, 0xb58b,	0xc60c, 0xd68d, 0xe70e, 0xf78f
	};
	
	while (length--) {
		crc = (crc >> 4) ^ crc16_table[(crc & 0xf) ^ (*data_p & 0xf)];
		crc = (crc >> 4) ^ crc16_table[(crc & 0xf) ^ (*data_p++ >> 4)];
	}
	
	crc = (crc << 8) | ((crc >> 8) & 0xff); // Swap endianness
	return (~crc);
}


HDLCDeframer::Config::Config() {
	mode = G3RUH;
	minimum_frame_length = 6;
	maximum_frame_length = 128;
	minimum_silence = 6 * 8;
	check_crc = true;
}


HDLCDeframer::HDLCDeframer(const Config& conf) :
	conf(conf),
	frame(256)
{
	if (conf.minimum_frame_length < 4)
		throw SuoError("HDLCDeframer: minimum_frame_length < 4");
	if (conf.minimum_frame_length > conf.maximum_frame_length)
		throw SuoError("HDLCDeframer: minimum_frame_length > conf.maximum_frame_length");

	reset();
}


void HDLCDeframer::reset()
{
	state = WaitingSync;
	shift = 0;
	stuffing_counter = 0;
	bit_idx = 0;
}


Symbol HDLCDeframer::descramble_bit(Symbol bit)
{
	if (conf.mode == G3RUH) {
		/* G3RUH descrambler */
		unsigned int lfsr_hi = (scrambler & 1);
		unsigned int lfsr_lo = ((scrambler >> 5) & 1);
		unsigned int descrambled_bit = (bit ^ (lfsr_hi ^ lfsr_lo)) != 0;

		scrambler = (scrambler >> 1);

		if (bit != 0)
			scrambler |= 0x00010000;

		bit = descrambled_bit;

		/* NRZI decode */
		Symbol new_bit = (bit != last_bit) ? 0 : 1;
		last_bit = bit;
		return new_bit;
	}
	else 
		return bit;

}

void HDLCDeframer::findStartFlag(Symbol bit, Timestamp now) {

	// More than 5 continious 1's have been received.
	if (stuffing_counter == 6 && bit == 0) { 
		// Start/end flag!
		syncDetected.emit(true, now);

		state = ReceivingFrame;
		frame.clear();
		shift = 0;
		stuffing_counter = 0;
		bit_idx = 0;

		// Start new frame
		frame.setMetadata("sync_timestamp", now);
	}

	stuffing_counter = bit ? (stuffing_counter + 1) : 0;
}


void HDLCDeframer::receivingFrame(Symbol bit, Timestamp now) {

	if (stuffing_counter >= 5) {
		// More than 5 continious 1's have been received.

		if (bit == 1) {
			// 6th 1 breaks the stuffing rule. End flag detected! 

			if (frame.data.size() < conf.minimum_frame_length) {
				// Repeated start flags
				bit_idx = 0;
				shift = 0;
				frame.data.clear();
				return;
			}

			syncDetected.emit(false, now);
			frame.setMetadata("completed_timestamp", now);

			if (conf.check_crc) {
				const size_t len = frame.data.size() - 2;
				const uint16_t received_crc = (frame.data[len] << 8) | frame.data[len + 1];
				const uint16_t calculated_crc = crc16_ccitt(&frame.data[0], len);

				if (received_crc == calculated_crc) {
					frame.data.resize(len); // Remove CRC
					sinkFrame.emit(frame, now);
				}

			}
			else {
				sinkFrame.emit(frame, now);
			}

			silence_counter = 0;
			state = Trailer;
			return;	
		}
		else {
			// More than 6 ones!
			// Unstuff the stuffing bit
			stuffing_counter = 0;
		}

	}
	else {

		stuffing_counter = bit ? (stuffing_counter + 1) : 0;

		shift = (0xFF & (shift << 1)) | bit;
		bit_idx++;

		if (bit_idx >= 8) {
			frame.data.push_back(shift);
			bit_idx = 0;
			shift = 0;

			// Too long frame
			if (frame.data.size() > conf.maximum_frame_length) {
				syncDetected.emit(false, now);
				reset();
			}
		}

	}
}


void HDLCDeframer::receivingTrailer(Symbol bit, Timestamp now) {

	if (stuffing_counter >= 5) {
		stuffing_counter = 0;
		silence_counter = 0;
		return;
	}
	else {
		stuffing_counter = 0;
	}

	stuffing_counter = bit ? (stuffing_counter + 1) : 0;
	silence_counter++;

	if (silence_counter >= conf.minimum_silence) {
		state = WaitingSync;
	}

}


void HDLCDeframer::sinkSymbol(Symbol bit, Timestamp now)
{
	bit = descramble_bit(bit);

	switch (state)
	{
	case WaitingSync:
		findStartFlag(bit, now);
		break;
	case ReceivingFrame:
		receivingFrame(bit, now);
		break;
	case Trailer:
		receivingTrailer(bit, now);
		break;
	default:
		throw SuoError("Invalid HDLCDeframer state!");
	}

}

Block* createHDLCDeframer(const Kwargs& args)
{
	return new HDLCDeframer();
}

static Registry registerHDLCDeframer("HDLCDeframer", &createHDLCDeframer);
