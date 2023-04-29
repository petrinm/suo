#include <iostream>

#include "syncword_deframer.hpp"
#include "registry.hpp"

using namespace suo;
using namespace std;


SyncwordDeframer::Config::Config() {
	syncword = 0x55f68d;
	syncword_len = 24;
	sync_threshold = 2;
	variable_length_frame = true;
	fixed_frame_length = 0;
}

SyncwordDeframer::SyncwordDeframer(const Config& conf) :
	conf(conf)
{
	if (conf.syncword_len > 8 * sizeof(conf.syncword))
		throw SuoError("Unrealistic syncword length");
	if (conf.variable_length_frame == false && conf.fixed_frame_length == 0)
		throw SuoError("..");
	syncword_mask = (1ULL << conf.syncword_len) - 1;
	reset();
}

void SyncwordDeframer::reset()
{
	syncDetected.emit(false, 0);
	state = Syncing;
	frame.clear();
	latest_bits = 0;
	bit_idx = 0;
	frame_len = 0;
}

void SyncwordDeframer::findSyncword(Symbol bit, Timestamp now) {
	
	latest_bits = (latest_bits << 1) | bit;

	unsigned int sync_errors = __builtin_popcountll((latest_bits & syncword_mask) ^ conf.syncword);
	if (sync_errors > conf.sync_threshold)
		return;

	// cout << "SYNC DETECTED! " << sync_errors << endl;

	/* Syncword found, start saving bits when next bit arrives */
	bit_idx = 0;
	latest_bits = 0;

	// Clear the frame and log metadata
	frame.clear();
	frame.id = rx_id_counter++;
	frame.timestamp = now;
	frame.setMetadata("sync_errors", sync_errors);
	frame.setMetadata("sync_timestamp", now);
	frame.setMetadata("sync_utc_timestamp", getCurrentISOTimestamp());

	syncDetected.emit(true, now);
	state = ReceivingHeader;
}

void SyncwordDeframer::receiveHeader(Symbol bit, Timestamp now) {
	latest_bits = (latest_bits << 1) | bit;
	if (++bit_idx < 8)
		return;

	// Decode Golay code
	frame_len = latest_bits;
	frame.data.resize(frame_len);

	// Clear for next state
	latest_bits = 0;
	bit_idx = 0;
	state = ReceivingPayload;
}

void SyncwordDeframer::receivePayload(Symbol bit, Timestamp now) {
	latest_bits = (latest_bits << 1) | bit;
	if (++bit_idx < 8)
		return;

	//cerr << std::hex << latest_bits << endl;

	frame.data.push_back(latest_bits);
	latest_bits = 0;
	bit_idx = 0;

	if (frame.data.size() < frame_len)
		return;

	// Receiving the frame completed
	state = Syncing;
	frame.setMetadata("completed_timestamp", now);

	syncDetected.emit(false, now);

	sinkFrame.emit(frame, now);

	frame.clear();
}

void SyncwordDeframer::sinkSymbol(Symbol bit, Timestamp now) {
	switch (state)
	{
	case Syncing:
		findSyncword(bit, now);
		break;
	case ReceivingHeader:
		receiveHeader(bit, now);
		break;
	case ReceivingPayload:
		receivePayload(bit, now);
		break;
	default:
		throw SuoError("Invalid SyncwordDeframer state!");
	}
}

void SyncwordDeframer::sinkSymbols(const SymbolVector& symbols, Timestamp now)
{
	for (const Symbol& symbol : symbols)
		sinkSymbol(symbol, now + 0);
}

void SyncwordDeframer::setMetadata(const std::string& name, const MetadataValue& value)
{
	frame.setMetadata(name, value);
}


Block* createSyncwordDeframer(const Kwargs& args)
{
	return new SyncwordDeframer();
}

static Registry registerSyncwordDeframer("SyncwordDeframer", &createSyncwordDeframer);
