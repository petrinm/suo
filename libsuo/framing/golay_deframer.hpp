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
		unsigned int syncword;

		/* Number of bits in syncword */
		unsigned int syncword_len;

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
	
	GolayDeframer(const GolayDeframer&) = delete;
	GolayDeframer& operator=(const GolayDeframer&) = delete;

	void reset();
	
	void sinkSymbol(Symbol bit, Timestamp time);
	void sinkSymbols(const std::vector<Symbol>& symbols, Timestamp timestamp);

	void setMetadata(const std::string& name, const MetadataValue& value);

	Port<Frame&, Timestamp> sinkFrame;
	Port<bool, Timestamp> syncDetected;

private:

	void findSyncword(Symbol bit, Timestamp now);
	void receiveHeader(Symbol bit, Timestamp now);
	void receivePayload(Symbol bit, Timestamp now);
	
	/* Configuration */
	Config conf;
	ReedSolomon rs;
	ConvolutionalEncoder conv_coder;
	uint64_t syncword_mask;

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

