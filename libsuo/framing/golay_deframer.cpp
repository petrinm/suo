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
	header_filter = GolayFramer::use_randomizer_flag | GolayFramer::use_reed_solomon_flag;
	skip_viterbi = false;
	skip_randomizer = false;
	skip_rs = false;
}

GolayDeframer::GolayDeframer(const Config& conf) :
	conf(conf),
	rs(RSCodes::CCSDS_RS_255_223),
	conv_coder(ConvolutionCodes::CCSDS_1_2_7)
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

	// Decode Golay code
	coded_len = latest_bits;
	int golay_errors = decode_golay24(&coded_len);
	if (golay_errors < 0)
	{
		cout << "Golay decode failed! " << endl;
		// TODO: Increase some counter
		reset();
		return;
	}

	// The 8 least signigicant bit indicate the lenght
	frame_len = 0xFF & coded_len;

	// Receive only packets which matches to filter
	if ((coded_len & 0xF00) != conf.header_filter) {
		cout << "Header filter!" << endl;
		reset();
		return;
	}

	frame.setMetadata("golay_errors", golay_errors);
	//frame.setMetadata("golay_coded", coded_len);

	// Receive double number of bits if viterbi is used
	if ((coded_len & GolayFramer::use_viterbi_flag) != 0) {
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

	if (conf.skip_viterbi == false && (coded_len & GolayFramer::use_viterbi_flag) != 0)
	{
		/* Decode viterbi */
		cerr << "Viterbi not yet implemented" << endl;
		reset();
		return;
	}

	if (conf.skip_randomizer == false && (coded_len & GolayFramer::use_randomizer_flag) != 0)
	{
		/* Scrambler the bytes */
		for (size_t i = 0; i < frame.data.size(); i++)
			frame.data[i] ^= ccsds_randomizer[i];
	}

	if (conf.skip_rs == false && (coded_len & GolayFramer::use_reed_solomon_flag) != 0)
	{
		/* Decode Reed-Solomon */
		try {
			unsigned int bytes_corrected = rs.decode(frame.data);
			unsigned int bits_corrected = 0; // count_bit_errors();
			frame.setMetadata("rs_bytes_corrected", bytes_corrected);
			frame.setMetadata("rs_bits_corrected", bits_corrected);
		}
		catch (ReedSolomonUncorrectable& e) {
			// TODO: Increment statistics
			cerr << "Reed-Solomon uncorrectable" << endl;
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
