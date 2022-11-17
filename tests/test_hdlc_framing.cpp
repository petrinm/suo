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
#include "utils.hpp"
#include "framing/hdlc_framer.hpp"
#include "framing/hdlc_deframer.hpp"

using namespace std;
using namespace suo;


class HDLCFramingTest : public CppUnit::TestFixture
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
		now = time(nullptr) % 0xFFFFFF;

		symbols.reserve(1024);
		symbols.clear();
		transmit_frame.clear();
		synced = false;
		unsynced = false;
	}

	void dummy_frame_sink(const Frame& frame, Timestamp _now) {
		(void)_now;
		received_frame = frame;
	}

	void dummy_frame_source(Frame& frame, Timestamp _now) {
		(void)_now;

		/* Create a test frame */
		transmit_frame.clear();
		transmit_frame.id = 2;
		transmit_frame.timestamp = 1234;
		transmit_frame.data.resize(28);
		for (size_t i = 0; i < 28; i++)
			transmit_frame.data[i] = i;

		cout << transmit_frame(Frame::PrintData | Frame::PrintColored);
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

		/*
		 * Enoding
		 */
		HDLCFramer::Config framer_conf;
		framer_conf.mode = HDLCFramer::G3RUH;
		framer_conf.preamble_length = 4;
		framer_conf.trailer_length = 2;
		framer_conf.append_crc = false;

		HDLCFramer framer(framer_conf);
		framer.sourceFrame.connect_member(this, &HDLCFramingTest::dummy_frame_source);

		/* Encode frame to bits */
		symbols.clear();
		framer.sourceSymbols(symbols, now);
		cout << "Output symbols: " << symbols.size() << endl;
		CPPUNIT_ASSERT(symbols.size() == 64 + 32 + 24 + 8 * 28);

		cout << "Symbols:" << endl;
		for (size_t i = 0; i < symbols.size(); i++) cout << symbols[i] << " ";
		cout << endl << endl;

		/*
		 * Decoding
		 */
		HDLCDeframer::Config deframer_conf;
		deframer_conf.mode = HDLCFramer::G3RUH;
		deframer_conf.check_crc = false;
		deframer_conf.maximum_frame_length = 256;
		deframer_conf.minimum_frame_length = 8;
		deframer_conf.minimum_silence = 5;

		HDLCDeframer deframer(deframer_conf);
		deframer.sinkFrame.connect_member(this, &HDLCFramingTest::dummy_frame_sink);
		deframer.syncDetected.connect_member(this, &HDLCFramingTest::dummy_sync_detected);

		received_frame.clear();

		/*
		* Feed some random symbols + bit from previous test
		*/
		unsigned int l = 50 + rand() % 50;
		for (size_t i = 0; i < l; i++)
			deframer.sinkSymbol(random_bit(), now++);
		for (size_t i = 0; i < 344; i++)
			deframer.sinkSymbol(symbols[i], now++);
		for (size_t i = 0; i < 100; i++)
			deframer.sinkSymbol(random_bit(), now++);

		CPPUNIT_ASSERT(synced == true);
		CPPUNIT_ASSERT(unsynced == true);
		CPPUNIT_ASSERT(received_frame.empty() == false);

		cout << received_frame(Frame::PrintData | Frame::PrintAltColor | Frame::PrintColored);

		//CPPUNIT_ASSERT(received_frame.timestamp != 0);
		CPPUNIT_ASSERT(received_frame.data.size() == transmit_frame.data.size());
		CPPUNIT_ASSERT(transmit_frame.data == received_frame.data);


		//ASSERT_METADATA_UINT(received_frame, METADATA_SYNC_ERRORS, 1);
		//ASSERT_METADATA_EXISTS(received_frame, METADATA_HDLC_CODED, METATYPE_UINT);
		//ASSERT_METADATA_UINT(received_frame, METADATA_HDLC_ERRORS, 1);

		// Check stats
		//CPPUNIT_ASSERT(dunno->frames_detected == 1);

	}


};


int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<HDLCFramingTest>("HDLCFramingTest", &HDLCFramingTest::basicTest));
	runner.run();
	return 0;
}
