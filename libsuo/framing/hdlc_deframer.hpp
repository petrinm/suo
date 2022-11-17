#pragma once

#include <memory>
#include "suo.hpp"

namespace suo
{

/*
 * CRC-16 CCITT algo
 */
extern uint16_t crc16_ccitt(const uint8_t* data_p, size_t length);

/*
 */
class HDLCDeframer : public Block
{
public:

	enum State {
		WaitingSync = 0,
		ReceivingFrame,
		Trailer,
	};

	struct Config
	{
		Config();

		/* NRZ, NRZ-I, G3RUH */
		unsigned int mode;

		/* Minimum frame length to filter false positive frames. */
		unsigned int minimum_frame_length;

		/* Maximum frame length to filter false positive frames. */
		unsigned int maximum_frame_length;

		/* Minimum silence time between frames in bits. */
		unsigned int minimum_silence;

		/* If true, */
		bool check_crc;
	};

	explicit HDLCDeframer(const Config& conf = Config());

	void reset();

	void sinkSymbol(Symbol bit, Timestamp now);

	Port<const Frame&, Timestamp> sinkFrame;
	Port<bool, Timestamp> syncDetected;

private:
	Symbol descramble_bit(Symbol bit);
	void findStartByte(Symbol bit, Timestamp now);
	void receivingFrame(Symbol bit, Timestamp now);
	void receivingTrailer(Symbol bit, Timestamp now);

	/* Configuration */
	Config conf;

	/* State */
	State state;
	unsigned int shift;
	unsigned int bit_idx;
	unsigned int stuffing_counter;
	Symbol last_bit;
	uint32_t scrambler;
	unsigned int silence_counter;

	Frame frame;
};

}; // namespace suo
