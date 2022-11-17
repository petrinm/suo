#pragma once

#include <memory>

#include "suo.hpp"
#include "coding/reed_solomon.hpp"
#include "coding/viterbi_decoder.hpp"

namespace suo
{


/*
 */
class GolayDeframer : public Block
{
public:

	enum State
	{
		Syncing = 0,
		ReceivingHeader,
		ReceivingPayload
	};

	struct Config
	{
		Config();

		/* Syncword */
		unsigned int sync_word;

		/* Number of bits in syncword */
		unsigned int sync_len;

		/* Maximum number of bit errors */
		unsigned int sync_threshold;

		/* Skip Reed-solomon coding */
		unsigned int skip_rs;

		/* Skip randomizer/scrambler */
		unsigned int skip_randomizer;

		/* Skip viterbi convolutional code */
		unsigned int skip_viterbi;
	};

	explicit GolayDeframer(const Config& conf = Config());
	
	void reset();
	
	void sinkSymbol(Symbol bit, Timestamp time);
	void sinkSymbols(const std::vector<Symbol>& symbols, Timestamp timestamp);

	Port<const Frame&, Timestamp> sinkFrame;
	Port<bool, Timestamp> syncDetected;

private:

	void findSyncword(Symbol bit, Timestamp time);
	void receiveHeader(Symbol bit, Timestamp time);
	void receivePayload(Symbol bit, Timestamp time);
	
	/* Configuration */
	Config conf;
	uint64_t sync_mask;

	/* State */
	State state;
	unsigned int latest_bits;
	unsigned int bit_idx;

	// Frame
	Frame frame;
	unsigned int frame_len;
	unsigned int coded_len;

};

}; // namespace suo

