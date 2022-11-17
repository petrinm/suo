#include <iostream>
#include <cstdlib> // rand()
#include <ctime> // time()
#include <cstring> // memcmp
#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "framing/golay_framer.hpp"
#include "framing/golay_deframer.hpp"
#include "utils.hpp"

using namespace std;
using namespace suo;


class GolayFramingTest : public CppUnit::TestFixture
{
private:
	Timestamp now;
	SymbolVector symbols;
	Frame received_frame;
	Frame transmit_frame;
	bool synced, unsynced;

public:

	void setUp() {
		srand(time(nullptr));
		now = rand() & 0xFFFFFF;

		symbols.reserve(2560);
		symbols.clear();
		synced = false;
		unsynced = false;
	}

	void dummy_frame_sink(const Frame &frame, Timestamp _now) {
		(void)_now;
		//cout << "dummy_frame_sink" << endl;
		received_frame = frame;
	}

	void dummy_frame_source(Frame& frame, Timestamp _now) {
		(void)_now;
		frame = transmit_frame;
	}

	void dummy_sync_detected(bool sync, Timestamp _now) {
		(void)_now;
		cout << "Sync detected = " << (sync ? "true" : "false") << endl;
		if (sync) synced = true;
		if (!sync) unsynced = true;
	}

	void tearDown() {
	}

	void basicTest()
	{

		size_t payload_len = 1 + (rand() % 255);

		/*
		 * Encoding test
		 */
		GolayFramer::Config framer_conf;
		framer_conf.sync_word = 0xdeadbeef;
		framer_conf.sync_len = 32;
		framer_conf.preamble_len = 64;
		framer_conf.use_viterbi = 0;
		framer_conf.use_randomizer = 1;
		framer_conf.use_rs = 0;

		GolayFramer framer(framer_conf);
		framer.sourceFrame.connect_member(this, &GolayFramingTest::dummy_frame_source);

		/* Create a test frame */
		transmit_frame.clear();
		transmit_frame.id = 2;
		transmit_frame.timestamp = 1234;
		transmit_frame.data.resize(payload_len);
		for (size_t i = 0; i < payload_len; i++)
			transmit_frame.data[i] = random_byte(); // i;
		cout << transmit_frame(Frame::PrintData | Frame::PrintColored);


		size_t total_symbols = framer_conf.preamble_len + framer_conf.sync_len + 24 + 8 * transmit_frame.data.size();

		
		/* Encode frame to bits */
		symbols.clear();
		framer.sourceSymbols(symbols, now);
		cout << "Output symbols: " << symbols.size() << endl;
		CPPUNIT_ASSERT(symbols.size() == total_symbols);

		/* Print out the generated symbols */
		cout << "Preamble:" << endl;
		for (size_t i = 0; i < 64; i++) cout << (int)symbols[i] << " ";
		cout << endl << endl;

		cout << "Sync:" << endl;
		for (size_t i = 64; i < 64 + 32; i++) cout << (int)symbols[i] << " ";
		cout << endl << endl;

		cout << "Golay:" << endl;
		for (size_t i = 64 + 32; i < 64 + 32 + 24; i++) cout << (int)symbols[i] << " ";
		cout << endl << endl;

		cout << "Data:" << endl;
		for (size_t i = 64 + 32 + 24; i < symbols.size(); i++) cout << (int)symbols[i] << " ";
		cout << endl << endl;


		/*
		* Decoding test
		*/

		symbols[64 + 2] ^= 1;  // Syncword error
		symbols[64 + 32 + 9] ^= 4;  // Golay bit error

		GolayDeframer::Config deframer_conf;
		deframer_conf.sync_word = framer_conf.sync_word;
		deframer_conf.sync_len = framer_conf.sync_len;
		deframer_conf.sync_threshold = 3;
		deframer_conf.skip_viterbi = 1;
		deframer_conf.skip_randomizer = 0;
		deframer_conf.skip_rs = 1;

		GolayDeframer deframer(deframer_conf);
		deframer.sinkFrame.connect_member(this, &GolayFramingTest::dummy_frame_sink);
		deframer.syncDetected.connect_member(this, &GolayFramingTest::dummy_sync_detected);

		received_frame.clear();

		/*
		* Feed some random symbols + bit from previous test
		*/
		unsigned int l = 50 + rand() % 50;
		for (size_t i = 0; i < l; i++)
			deframer.sinkSymbol(random_bit(), now++);
		for (size_t i = 0; i < symbols.size(); i++)
			deframer.sinkSymbol(symbols[i], now++);
		for (size_t i = 0; i < 100; i++)
			deframer.sinkSymbol(random_bit(), now++);

		CPPUNIT_ASSERT(synced == true);
		CPPUNIT_ASSERT(unsynced == true);
		CPPUNIT_ASSERT(received_frame.empty() == false);
		cout << received_frame(Frame::PrintData | Frame::PrintAltColor | Frame::PrintColored);

		//CPPUNIT_ASSERT(received_frame.h received_framedr.timestamp != 0);
		CPPUNIT_ASSERT(received_frame.data.size() == transmit_frame.data.size());
		//CPPUNIT_ASSERT(memcmp(transmit_frame.data, received_frame.data, received_frame.data.size()) == 0);
		CPPUNIT_ASSERT(transmit_frame.data == received_frame.data);


		//ASSERT_METADATA_UINT(received_frame, METADATA_SYNC_ERRORS, 1);
		//ASSERT_METADATA_EXISTS(received_frame, METADATA_GOLAY_CODED, METATYPE_UINT);
		//ASSERT_METADATA_UINT(received_frame, METADATA_GOLAY_ERRORS, 1);

		// Check stats
		//CPPUNIT_ASSERT(dunno->frames_detected == 1);

	}

};


int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<GolayFramingTest>("GolayFramingTest", &GolayFramingTest::basicTest));
	runner.run();
	return 0;
}
