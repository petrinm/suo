#include <iostream>


#include "framing/golay_deframer.hpp"
#include "coding/golay24.hpp"
#include "coding/randomizer.hpp"
#include "registry.hpp"


using namespace std;
using namespace suo;


GolayDeframer::Config::Config() {
	sync_word = 0xC9D08A7B;
	sync_len = 32;
	sync_threshold = 3;
	skip_viterbi = 0;
	skip_randomizer = 0;
	skip_rs = 0;
}

GolayDeframer::GolayDeframer(const Config& conf) :
	conf(conf)
{
	sync_mask = (1ULL << conf.sync_len) - 1;
	reset();
}

void GolayDeframer::reset()
{
	state = Syncing;
	latest_bits = 0;
	frame.clear();
	frame_len = 0;
	coded_len = 0;
}

void GolayDeframer::findSyncword(Symbol bit, Timestamp now)
{
	/*
	 * Looking for syncword
	 */

	latest_bits = (latest_bits << 1) | bit;	

#if 0
	// Check for inverse of the syncword
	if ((latest_bits & sync_mask) == (conf.sync_word ^ sync_mask))
		cout << "Inversed syncword detected!" << endl;
#endif

	unsigned int sync_errors = __builtin_popcountll((latest_bits & sync_mask) ^ conf.sync_word);
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

	syncDetected.emit(true, now);
	state = ReceivingHeader;
}

/*
 * Receiveiving PHY header (length bytes)
 */
void GolayDeframer::receiveHeader(Symbol bit, Timestamp now)
{
	latest_bits = (latest_bits << 1) | bit;
	if (++bit_idx < 24)
		return;

	// Decode Golay code
	coded_len = latest_bits;
	int golay_errors = decode_golay24(&coded_len);
	if (golay_errors < 0)
	{
		// cout << "GOLAY failed!" << endl;
		//  TODO: Increase some counter
		state = Syncing;
		syncDetected.emit(false, now);
	}

	// cout << "GOLAY errors " << golay_errors << endl;
	//  The 8 least signigicant bit indicate the lenght
	frame_len = 0xFF & coded_len;

	// Sanity
	if ((frame_len & 0x00) != 0)
	{
		state = Syncing;
		syncDetected.emit(false, now);
	}	

	frame.setMetadata("golay_errors", golay_errors);
	//frame.setMetadata("golay_coded", coded_len);

	// Receive double number of bits if viterbi is used
	if ((coded_len & 0x800) != 0)
		frame_len *= 2;

	// Clear for next state
	latest_bits = 0;
	bit_idx = 0;
	state = ReceivingPayload;
}

/*
 * Receiving payload state
 */
void GolayDeframer::receivePayload(Symbol bit, Timestamp now)
{
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

	if (conf.skip_viterbi == 0 && (coded_len & 0x800) != 0)
	{
		/* Decode viterbi */
		throw SuoError("Viterbi not yet implemented");
	}

	if (conf.skip_randomizer == 0 && (coded_len & 0x400) != 0)
	{
		/* Scrambler the bytes */
		for (size_t i = 0; i < frame.data.size(); i++)
			frame.data[i] ^= ccsds_randomizer[i];
	}

	if (conf.skip_rs == 0 && (coded_len & 0x200) != 0)
	{
		/* Decode Reed-Solomon */
		throw SuoError("Reed-Solomon not yet implemented");
	}

	syncDetected.emit(false, now);
	
	sinkFrame.emit(frame, now);

	frame.clear();
}


void GolayDeframer::sinkSymbol(Symbol bit, Timestamp now)
{
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
		throw SuoError("Invalid GolayDeframer state!");
	}
}


void GolayDeframer::sinkSymbols(const std::vector<Symbol> &symbols, Timestamp timestamp)
{
	for (const Symbol& symbol: symbols)
		sinkSymbol(symbol, timestamp);
}

Block* createGolayDeframer(const Kwargs &args)
{
	return new GolayDeframer();
}

static Registry registerGolayDeframer("GolayDeframer", &createGolayDeframer);
