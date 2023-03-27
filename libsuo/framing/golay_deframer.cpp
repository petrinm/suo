#include <iostream>

#include "framing/golay_deframer.hpp"
#include "framing/golay_framer.hpp"
#include "coding/golay24.hpp"
#include "coding/randomizer.hpp"

#include "registry.hpp"


using namespace std;
using namespace suo;


GolayDeframer::Config::Config() {
	syncword = 0xC9D08A7B;
	syncword_len = 32;
	sync_threshold = 3;
	use_viterbi = false;
	use_randomizer = false;
	use_rs = false;
	legacy_mode = false;
}

GolayDeframer::GolayDeframer(const Config& conf) :
	conf(conf),
	rs(RSCodes::CCSDS_RS_255_223)
//	viterbi(ConvolutionCodes::CCSDS_1_2_7)
{
	if (conf.syncword_len > 8 * sizeof(conf.syncword))
		throw SuoError("Unrealistic syncword length");

	syncword_mask = (1ULL << conf.syncword_len) - 1;
	reset();
}

void GolayDeframer::reset()
{
	syncDetected.emit(false, 0);
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
	if ((latest_bits & syncword_mask) == (conf.syncword ^ syncword_mask))
		cout << "Inversed syncword detected!" << endl;
#endif

	unsigned int sync_errors = __builtin_popcountll((latest_bits & syncword_mask) ^ conf.syncword);
	if (sync_errors > conf.sync_threshold)
		return;

	//cout << "SYNC DETECTED! " << sync_errors << endl;

	/* Syncword found, start saving bits when next bit arrives */
	bit_idx = 0;
	latest_bits = 0;

	// Clear the frame and log metadata
	frame.clear();
	frame.id = rx_id_counter++;
	frame.timestamp = now;
	frame.setMetadata("sync_errors", sync_errors);
	frame.setMetadata("sync_timestamp", now);
	frame.setMetadata("sync_utc_timestamp", getISOCurrentTimestamp());

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

#if 0
	if (conf.legacy_mode == false) {
		// If Reed Solomon in non-legacy the frame cannot be longer than 255 bytes.
		// Thus, we can zero out the header's MSBs before decoding golay for possible sensitivity.
		if (conf.use_rs)
			latest_bits &= 0xFFF0FF;
	}
#endif

	// Decode Golay code
	coded_len = latest_bits;
	int golay_errors = decode_golay24(&coded_len);
	if (golay_errors < 0)
	{
		cerr << "Golay decode failed! " << endl;
		// TODO: Increase some counter
		reset();
		return;
	}

	// Decode frame length
	if (conf.legacy_mode) {
		// The 8 least signigicant bit indicate the length and 
		// upper 4 bits are used for indicating the mode (Viterbi,Randomizer,RS).
		frame_len = 0xFF & coded_len;
	}
	else {
		frame_len = 0xFFF & coded_len;
	}

	// In any case if RS is used, the length cannot be shorter than RS number of parity bytes or longer than the RS message length. 
	if (conf.use_rs && frame_len < (32 + 1) && frame_len > 255) {
		cerr << "Invalid frame length!" << endl;
		reset();
		return;
	}

	frame.setMetadata("golay_errors", golay_errors);
	//frame.setMetadata("golay_coded", coded_len);

	// Receive double number of bits if viterbi is used
	if (conf.legacy_mode ? ((coded_len & GolayFramer::use_viterbi_flag) != 0) : conf.use_viterbi)  {
		frame_len *= 2;
		cerr << "Viterbi not yet implemented" << endl;
		reset();
		return;
	}

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

	// Receiving the frame completed?
	if (frame.data.size() < frame_len)
		return;
		
	frame.setMetadata("completed_timestamp", now);
	frame.setMetadata("completed_utc_timestamp", getISOCurrentTimestamp());

	if (conf.legacy_mode ? ((coded_len & GolayFramer::use_viterbi_flag) != 0) : conf.use_viterbi)
	{
		/* Decode viterbi */
		cerr << "Viterbi not yet implemented" << endl;
		reset();
		return;
	}

	//if // U482C mode
	if (conf.legacy_mode ? ((coded_len & GolayFramer::use_randomizer_flag) != 0) : conf.use_randomizer)
	{
		/* Scrambler the bytes */
		for (size_t i = 0; i < frame.data.size(); i++)
			frame.data[i] ^= ccsds_tm_randomizer[i];
	}

	if (conf.legacy_mode ? ((coded_len & GolayFramer::use_reed_solomon_flag) != 0) : conf.use_rs)
	{
		/* Decode Reed-Solomon */
		try {
			ByteVector original = frame.data;
			unsigned int bytes_corrected = rs.decode(frame.data);
			unsigned int bits_corrected = count_bit_errors(frame.data, original);
			frame.setMetadata("rs_bytes_corrected", bytes_corrected);
			frame.setMetadata("rs_bits_corrected", bits_corrected);
		}
		catch (SuoError& e) {
			// TODO: Increment some statistics
			cerr << "Reed-Solomon failed: " << e.what() << endl;
			reset();
			return;
		}
	}

	syncDetected.emit(false, now);
	sinkFrame.emit(frame, now);

	reset();
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


void GolayDeframer::sinkSymbols(const std::vector<Symbol> &symbols, Timestamp now)
{
	for (const Symbol& symbol: symbols)
		sinkSymbol(symbol, now);
}

void GolayDeframer::setMetadata(const std::string& name, const MetadataValue& value) {
	frame.setMetadata(name, value);
}

Block* createGolayDeframer(const Kwargs &args)
{
	return new GolayDeframer();
}

static Registry registerGolayDeframer("GolayDeframer", &createGolayDeframer);
