#pragma once

#include "suo.hpp"

namespace suo {

/*
 * Syncword deframer
 * Support fixed length frames or frames with length byte
 */
class SyncwordDeframer : public Block
{
public:
	
	enum State
	{
		Syncing = 0,
		ReceivingHeader,
		ReceivingPayload
	};
	
	struct Config {
		Config();

		/* Syncword */
		unsigned int syncword;

		/* Number of bits in syncword */
		unsigned int syncword_len;

		/* Maximum number of bit errors */
		unsigned int sync_threshold;

		/* If true,  */
		bool variable_length_frame;

		/* Fixed frame length. */
		unsigned int fixed_frame_length;

	};

	explicit SyncwordDeframer(const Config& conf = Config());

	SyncwordDeframer(const SyncwordDeframer&) = delete;
	SyncwordDeframer& operator=(const SyncwordDeframer&) = delete;

	void reset();

	void sinkSymbol(Symbol bit, Timestamp now);
	void sinkSymbols(const SymbolVector& symbols, Timestamp now);

	void setMetadata(const std::string& name, const MetadataValue& value);

	Port<Frame&, Timestamp> sinkFrame;
	Port<bool, Timestamp> syncDetected;

private:

	void findSyncword(Symbol bit, Timestamp now);
	void receiveHeader(Symbol bit, Timestamp now);
	void receivePayload(Symbol bit, Timestamp now);

	/* Configuration */
	const Config conf;
	uint32_t syncword_mask;

	/* State */
	State state;
	unsigned int latest_bits;
	unsigned int bit_idx;
	Frame frame;
	unsigned int frame_len;
};

} // namespace suo
