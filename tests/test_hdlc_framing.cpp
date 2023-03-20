#include <iostream>
#include <cstdlib> // rand()
#include <ctime> // time()
#include <cstring> // memcmp
#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <suo.hpp>
#include <framing/hdlc_framer.hpp>
#include <framing/hdlc_deframer.hpp>

#include "utils.hpp"

using namespace std;
using namespace suo;


class HDLCFramingTest : public CppUnit::TestFixture
{
private:
	Timestamp now;

public:

	void setUp() {
		srand(time(nullptr));
		now = time(nullptr) % 0xFFFFFF;
	}

	void basicTest()
	{

		/*
		 * Enoding
		 */
		HDLCFramer::Config framer_conf;
		framer_conf.mode = HDLCMode::G3RUH;
		framer_conf.preamble_length = 10;
		framer_conf.trailer_length = 2;
		framer_conf.append_crc = false;
		HDLCFramer framer(framer_conf);

		RandomFrameGenerator frame_generator(28);
		framer.sourceFrame.connect_member(&frame_generator, &RandomFrameGenerator::source_frame);


		/* Encode frame to bits */
		SymbolVector symbols;
		symbols.reserve(1024);
		SymbolGenerator gen = framer.generateSymbols(now);
		gen.sourceSymbols(symbols);

		size_t data_len = 28;
		size_t min_size = 8 * (framer_conf.preamble_length + data_len + framer_conf.trailer_length);
		CPPUNIT_ASSERT(symbols.size() >= min_size);
		//CPPUNIT_ASSERT(symbols.size() < min_size + data_len);

		//cout << "Symbols:" << endl << symbols << endl << endl;

		/*
		 * Decoding
		 */
		HDLCDeframer::Config deframer_conf;
		deframer_conf.mode = framer_conf.mode;
		deframer_conf.check_crc = false;
		deframer_conf.maximum_frame_length = 256;
		deframer_conf.minimum_frame_length = 8;
		deframer_conf.minimum_silence = 5;

		HDLCDeframer deframer(deframer_conf);

		Frame received_frame;
		deframer.sinkFrame.connect([&](Frame& frame, Timestamp now) {
			(void) now;
			received_frame = frame;
		});

		bool synced = false, unsynced = false;
		deframer.syncDetected.connect( [&](bool sync, Timestamp now) {
			(void)now;
			//cout << "Sync detected = " << (sync ? "true" : "false") << endl;
			if (sync) synced = true;
			if (!sync) unsynced = true;
		});

		/*
		* Feed some random symbols + bit from previous test
		*/
		unsigned int l = 16 + rand() % 50;
		for (size_t i = 0; i < 16; i++)
			deframer.sinkSymbol(random_bit(), now++);
		for (size_t i = 0; i < symbols.size(); i++)
			deframer.sinkSymbol(symbols[i], now++);
		//for (size_t i = 0; i < 100; i++)
		//	deframer.sinkSymbol(random_bit(), now++);

		CPPUNIT_ASSERT(synced == true);
		CPPUNIT_ASSERT(unsynced == true);
		CPPUNIT_ASSERT(received_frame.empty() == false);

		cout << received_frame(Frame::PrintData | Frame::PrintAltColor | Frame::PrintColored);

		//CPPUNIT_ASSERT(received_frame.timestamp != 0);
		CPPUNIT_ASSERT(received_frame.data.size() == frame_generator.latest_frame().data.size());
		CPPUNIT_ASSERT(frame_generator.latest_frame().data == received_frame.data);


		//ASSERT_METADATA_UINT(received_frame, METADATA_SYNC_ERRORS, 1);
		//ASSERT_METADATA_EXISTS(received_frame, METADATA_HDLC_CODED, METATYPE_UINT);
		//ASSERT_METADATA_UINT(received_frame, METADATA_HDLC_ERRORS, 1);

		// Check stats
		//CPPUNIT_ASSERT(dunno->frames_detected == 1);

	}


	void crcTest() {

		/*
		 * Enoding
		 */
		HDLCFramer::Config framer_conf;
		framer_conf.mode = HDLCMode::G3RUH;
		framer_conf.preamble_length = 10;
		framer_conf.trailer_length = 2;
		framer_conf.append_crc = true;
		HDLCFramer framer(framer_conf);

		RandomFrameGenerator frame_generator(10, 100);
		framer.sourceFrame.connect_member(&frame_generator, &RandomFrameGenerator::source_frame);

		/* Encode frame to bits */
		SymbolVector symbols;
		symbols.reserve(1024);
		SymbolGenerator symbol_gen = framer.generateSymbols(now);
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		symbol_gen.sourceSymbols(symbols);

		size_t data_len = frame_generator.latest_frame().size();
		size_t calc_frame_size = 8 * (framer_conf.preamble_length + data_len + framer_conf.trailer_length);
		CPPUNIT_ASSERT(symbols.size() >= calc_frame_size);

		/*
		 * Decoding
		 */
		HDLCDeframer::Config deframer_conf;
		deframer_conf.mode = framer_conf.mode;
		deframer_conf.check_crc = true;
		deframer_conf.maximum_frame_length = 256;
		deframer_conf.minimum_frame_length = 8;
		deframer_conf.minimum_silence = 5;

		HDLCDeframer deframer(deframer_conf);

		Frame received_frame;
		deframer.sinkFrame.connect([&](Frame& frame, Timestamp now) {
			received_frame = frame;
		});

		bool synced = false, unsynced = false;
		deframer.syncDetected.connect( [&](bool sync, Timestamp now) {
			//cout << "Sync detected = " << (sync ? "true" : "false") << endl;
			if (sync) synced = true;
			if (!sync) unsynced = true;
		});

	

		/*
		* Feed some random symbols + bit from previous test
		*/
		unsigned int l = 16 + rand() % 50;
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

		//CPPUNIT_ASSERT(received_frame.timestamp != 0);
		CPPUNIT_ASSERT(received_frame.data.size() == frame_generator.latest_frame().data.size());
		CPPUNIT_ASSERT(frame_generator.latest_frame().data == received_frame.data);


		//ASSERT_METADATA_UINT(received_frame, METADATA_SYNC_ERRORS, 1);
		//ASSERT_METADATA_EXISTS(received_frame, METADATA_HDLC_CODED, METATYPE_UINT);
		//ASSERT_METADATA_UINT(received_frame, METADATA_HDLC_ERRORS, 1);

		// Check stats
		//CPPUNIT_ASSERT(dunno->frames_detected == 1);
	}

	void testMissingZeroInSync() {
		// Testaa että "jaettu nolla" preamblessa toimii  011111101111110

		const SymbolVector bits = {
			// Random
			0, 0, 1, 0, 0, 1, 1, 0,
			1, 0, 1, 1, 0, 0, 1, 0,
			1, 1, 1, 1, 1, 1, 0, 0,
			1, 1, 0, 1, 1, 0, 1, 1,
			0, 1, 1, 1, 0, 0, 1, 1, 
			1, 0, 0,
			// Start flags with missing zero
			1, 1, 1, 1, 1, 1, 0,
			1, 1, 1, 1, 1, 1, 0,
			1, 1, 1, 1, 1, 1, 0,
			1, 1, 1, 1, 1, 1, 0,
			// Data
			0, 0, 0, 1, 0, 0, 0, 1,
			0, 0, 1, 0, 0, 0, 1, 0,
			0, 0, 1, 1, 0, 0, 1, 1,
			0, 1, 0, 0, 0, 1, 0, 0,
			0, 1, 0, 1, 0, 1, 0, 1,
			0, 1, 1, 0, 0, 1, 1, 0,
			0, 1, 1, 1, 0, 1, 1, 1,
			// End flags with missing zero
			1, 1, 1, 1, 1, 1, 0,
			0, 1, 1, 1, 1, 1, 1, 0,
			0, 1, 1, 1, 1, 1, 1, 0,
		};

		HDLCDeframer::Config deframer_conf;
		deframer_conf.mode = HDLCMode::Uncoded;
		deframer_conf.check_crc = true;
		deframer_conf.maximum_frame_length = 256;
		deframer_conf.minimum_frame_length = 8;
		deframer_conf.minimum_silence = 5;

		HDLCDeframer deframer(deframer_conf);

		Frame received_frame;
		deframer.sinkFrame.connect([&](Frame& frame, Timestamp now) {
			received_frame = frame;
		});

		bool synced = false, unsynced = false;
		deframer.syncDetected.connect( [&](bool sync, Timestamp now) {
			//cout << "Sync detected = " << (sync ? "true" : "false") << endl;
			if (sync) synced = true;
			if (!sync) unsynced = true;
		});


		for (Symbol b: bits)
			deframer.sinkSymbol(b, now);
		
		CPPUNIT_ASSERT(synced == true);
		CPPUNIT_ASSERT(unsynced == true);
		CPPUNIT_ASSERT(received_frame.size() == 7);

		CPPUNIT_ASSERT(received_frame.data[0] == 0x11);
		CPPUNIT_ASSERT(received_frame.data[1] == 0x22);
		CPPUNIT_ASSERT(received_frame.data[2] == 0x33);
		CPPUNIT_ASSERT(received_frame.data[3] == 0x44);
		CPPUNIT_ASSERT(received_frame.data[4] == 0x55);
		CPPUNIT_ASSERT(received_frame.data[5] == 0x66);
		CPPUNIT_ASSERT(received_frame.data[6] == 0x77);

	}

	void testGeneratingInSmallChunks() {

		/* Setup HDLC framer */
		HDLCFramer::Config framer_conf;
		framer_conf.mode = HDLCMode::G3RUH;
		framer_conf.preamble_length = 10;
		framer_conf.trailer_length = 2;
		framer_conf.append_crc = false;
		HDLCFramer framer(framer_conf);

		/* Setup random frame generator */
		unsigned int seed = time(nullptr);
		RandomFrameGenerator frame_generator(28);
		frame_generator.set_seed(seed);
		framer.sourceFrame.connect_member(&frame_generator, &RandomFrameGenerator::source_frame);

		/* Encode a random frame to one long bit vector */
		SymbolVector all_symbols(1024);
		SymbolGenerator symbol_gen = framer.generateSymbols(now);
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		symbol_gen.sourceSymbols(all_symbols);
		CPPUNIT_ASSERT(symbol_gen.running() == false);
		CPPUNIT_ASSERT(all_symbols.size() > 0);
		CPPUNIT_ASSERT((all_symbols.flags & VectorFlags::start_of_burst) != 0);
		CPPUNIT_ASSERT((all_symbols.flags & VectorFlags::end_of_burst) != 0);

		/* Encode the same random frame to bits in smaller chunks of bit vector */
		for (unsigned int chunk_size = 1; chunk_size < 16; chunk_size++) {

			framer.reset();
			frame_generator.set_seed(seed);
			SymbolGenerator symbol_gen = framer.generateSymbols(now);
			CPPUNIT_ASSERT(symbol_gen.running() == true);

			SymbolVector symbols(chunk_size);
			unsigned int symbols_idx = 0;
			while (symbols_idx < all_symbols.size()) {

				CPPUNIT_ASSERT(symbol_gen.running() == true);

				symbols.clear();
				symbol_gen.sourceSymbols(symbols);
				
				/* Check that outputted symbols are same as in the long bit vector */
				CPPUNIT_ASSERT(symbols.size() <= chunk_size);
				for (unsigned int i = 0; i < symbols.size(); i++)
					CPPUNIT_ASSERT(symbols[i] == all_symbols[symbols_idx + i]);

				symbols_idx += symbols.size();
			}

			// TODO: CPPUNIT_ASSERT(symbol_gen.running() == false);
			CPPUNIT_ASSERT(symbols_idx == all_symbols.size());

			// Make sure over sourcing won't crash
			symbol_gen.sourceSymbols(symbols);
		}

	}

	void testGeneratingWithIterator() {
		/* Setup HDLC framer */
		HDLCFramer::Config framer_conf;
		framer_conf.mode = HDLCMode::G3RUH;
		framer_conf.preamble_length = 10;
		framer_conf.trailer_length = 2;
		framer_conf.append_crc = false;
		HDLCFramer framer(framer_conf);

		/* Setup random frame generator */
		unsigned int seed = time(nullptr);
		RandomFrameGenerator frame_generator(28);
		frame_generator.set_frame_count(2);
		frame_generator.set_seed(seed);
		framer.sourceFrame.connect_member(&frame_generator, &RandomFrameGenerator::source_frame);


		SymbolVector all_symbols(1024);

		/* Encode a random frame to one long bit vector */
		{
			SymbolGenerator symbol_gen = framer.generateSymbols(now);
			CPPUNIT_ASSERT(symbol_gen.running() == true);
			symbol_gen.sourceSymbols(all_symbols);
			CPPUNIT_ASSERT(symbol_gen.running() == false);
			CPPUNIT_ASSERT(all_symbols.size() > 0);
			CPPUNIT_ASSERT((all_symbols.flags & VectorFlags::start_of_burst) != 0);
			CPPUNIT_ASSERT((all_symbols.flags & VectorFlags::end_of_burst) != 0);
		}

		/* Encode the same random frame to bits in smaller chunks of bit vector */
		framer.reset();
		frame_generator.set_seed(seed);
		{
			SymbolGenerator symbol_gen = framer.generateSymbols(now);
			CPPUNIT_ASSERT(symbol_gen.running() == true);

			SymbolVector generated_symbols;
			generated_symbols.reserve(1024);
			for (Symbol s : symbol_gen)
				generated_symbols.push_back(s);

			// TODO: CPPUNIT_ASSERT(symbol_gen.running() == false);
			CPPUNIT_ASSERT(generated_symbols.size() == all_symbols.size());
			CPPUNIT_ASSERT(generated_symbols == all_symbols);
		}
	}


	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("HDLCFramingTest");
		suite->addTest(new CppUnit::TestCaller<HDLCFramingTest>("Basic test", &HDLCFramingTest::basicTest));
		suite->addTest(new CppUnit::TestCaller<HDLCFramingTest>("CRC test", &HDLCFramingTest::crcTest));
		suite->addTest(new CppUnit::TestCaller<HDLCFramingTest>("Missing zero in sync", &HDLCFramingTest::testMissingZeroInSync));
		suite->addTest(new CppUnit::TestCaller<HDLCFramingTest>("Generating in small chunks", &HDLCFramingTest::testGeneratingInSmallChunks));
		suite->addTest(new CppUnit::TestCaller<HDLCFramingTest>("Generating with iterator", &HDLCFramingTest::testGeneratingWithIterator));
		return suite;
	}


};


#ifndef COMBINED_TEST
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(HDLCFramingTest::suite());
	runner.run();
	return 0;
}
#endif
