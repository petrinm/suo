#pragma once

#include <memory>
#include "suo.hpp"

namespace suo
{

/*
 * CRC-16 CCITT algo
 */
extern uint16_t crc16_ccitt(const uint8_t* data_p, size_t length);

enum HDLCMode {
	Plain,
	G3RUH,
};


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
		HDLCMode mode;

		/* Minimum frame length to filter false positive frames. */
		unsigned int minimum_frame_length;

		/* Maximum frame length to filter false positive frames. */
		unsigned int maximum_frame_length;

		/* Minimum silence time between frames in bits. */
		unsigned int minimum_silence;

		/* If true, only frames passing CRC data integrity check will be forwarded. */
		bool check_crc;
	};

	explicit HDLCDeframer(const Config& conf = Config());

	HDLCDeframer(const HDLCDeframer&) = delete;
	HDLCDeframer& operator=(const HDLCDeframer&) = delete;

	void reset();

	void sinkSymbol(Symbol bit, Timestamp now);
	//void sinkSymbols(const SymbolVector& symbols, Timestamp timestamp);

	Port<Frame&, Timestamp> sinkFrame;
	Port<bool, Timestamp> syncDetected;

private:
	Symbol descramble_bit(Symbol bit);
	void findStartFlag(Symbol bit, Timestamp now);
	void receivingFrame(Symbol bit, Timestamp now);
	void receivingTrailer(Symbol bit, Timestamp now);

	/* Configuration */
	Config conf;

	/* State */
	State state;
	unsigned int shift;
	unsigned int bit_idx;
	unsigned int silence_counter;
	Frame frame;

	// Scrambler state
	Symbol last_bit;
	uint32_t scrambler;
	unsigned int stuffing_counter;

};

}; // namespace suo
