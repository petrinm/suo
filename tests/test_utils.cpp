#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "framing/utils.hpp"

using namespace std;
using namespace suo;


class FrameTest: public CppUnit::TestFixture
{
private:
	Timestamp now;

public:

	void setUp() {
		srand(time(nullptr));
		now = time(nullptr) % 0xFFFFFF;
	}


	void testMetadata() {

		cout << getISOCurrentTimestamp() << endl;

		/* Create a test frame */
		Frame frame_a;
		frame_a.id = rand() & 0xffff;
		frame_a.timestamp = now;
		frame_a.flags |= Frame::Flags::has_timestamp;
		frame_a.data.resize(rand() % 256);
		for (size_t i = 0; i < frame_a.size(); i++)
			frame_a.data[i] = rand() % 256;

		frame_a.setMetadata("SYNC_ERRORS", -1);
		frame_a.setMetadata("GOLAY_CODED", 1337);
		frame_a.setMetadata("CFO", 12.345);
		frame_a.setMetadata("RSSI", -123.4);
		CPPUNIT_ASSERT(frame_a.metadata.size() == 4);

		cout << frame_a(Frame::PrintData | Frame::PrintMetadata | Frame::PrintAltColor);

		std::string json = frame_a.serialize_to_json();
		cout << json << endl;

		// Deserialize frame
		Frame frame_b = Frame::deserialize_from_json(json);
		cout << frame_a(Frame::PrintData | Frame::PrintMetadata | Frame::PrintAltColor);

	}

	void json_parsing_test() {

		try {
			Frame frame = Frame::deserialize_from_json("{ \"data\": \"0123\" }");
		}
		catch (const SuoError& e) {
			cerr << "SuoError: " << e.what() << endl;
		}
	

		/* Faulty JSON message: Invalid JSON due to missing quote */
		{
			bool caught_exception = false;
			try {
				Frame frame = Frame::deserialize_from_json("{ \"data\": \"0123\" ");
			}
			catch (const SuoError& e) {
				cerr << "SuoError: " << e.what() << endl;
				caught_exception = true;
			}
			CPPUNIT_ASSERT(caught_exception);
		}

		/* Faulty JSON message: Odd number of characters in data string */
		{
			bool caught_exception = false;
			try {
				Frame frame = Frame::deserialize_from_json("{ \"data\": \"01234\" }");
			}
			catch (const SuoError& e) {
				cerr << "SuoError: " << e.what() << endl;
				caught_exception = true;
			}
			CPPUNIT_ASSERT(caught_exception);
		}

		/* Faulty JSON message: Invalid character in data string */
		{
			bool caught_exception = false;
			try {
				Frame frame = Frame::deserialize_from_json("{ \"data\": \"12xx34\" }");
			}
			catch (const std::exception& e) {
				cerr << "std::exception: " << e.what() << endl;
				caught_exception = true;
			}
			CPPUNIT_ASSERT(caught_exception);
		}

		/* Faulty JSON message: */
		{
			bool caught_exception = false;
			try {
				Frame frame = Frame::deserialize_from_json("{ \"data\": \"120x34\", \"metadata\": 123 }");
			}
			catch (const SuoError& e) {
				cerr << "SuoError: " << e.what() << endl;
				caught_exception = true;
			}
			CPPUNIT_ASSERT(caught_exception);
		}
	}


	void test_bit_operations() {

		/*
		size_t bytes_to_bits(bit_t *bits, const uint8_t *bytes, size_t nbytes, bool lsb_first);
		size_t word_to_lsb_bits(bit_t *bits, size_t nbits, uint64_t word);
		size_t word_to_msb_bits(bit_t *bits, size_t nbits, uint64_t word);
		*/
	}

	/* Test reverse bit function */
	void test_reverse_bits() {

		// UINT8_T
		CPPUNIT_ASSERT(reverse_bits((uint8_t)0x00) == 0x00);
		CPPUNIT_ASSERT(reverse_bits((uint8_t)0x01) == 0x80);
		CPPUNIT_ASSERT(reverse_bits((uint8_t)0x40) == 0x02);
		CPPUNIT_ASSERT(reverse_bits((uint8_t)0x03) == 0xC0);
		CPPUNIT_ASSERT(reverse_bits((uint8_t)0xFE) == 0x7F);
		CPPUNIT_ASSERT(reverse_bits((uint8_t)0xFF) == 0xFF);

		// UINT16_T
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0x0000) == 0x0000);
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0x0010) == 0x0800);
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0x0E03) == 0xC070);
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0x0007) == 0xE000);
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0x8181) == 0x8181);
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0xFEFE) == 0x7F7F);
		CPPUNIT_ASSERT(reverse_bits((uint16_t)0xFFFF) == 0xFFFF);

		// UINT32_T
		CPPUNIT_ASSERT(reverse_bits((uint32_t)0x00000000) == 0x00000000);
		CPPUNIT_ASSERT(reverse_bits((uint32_t)0x00800001) == 0x80000100);
		CPPUNIT_ASSERT(reverse_bits((uint32_t)0x01000400) == 0x00200080);
		CPPUNIT_ASSERT(reverse_bits((uint32_t)0x00030008) == 0x1000C000);
		CPPUNIT_ASSERT(reverse_bits((uint32_t)0xFFFEFFFE) == 0x7FFF7FFF);
		CPPUNIT_ASSERT(reverse_bits((uint32_t)0xFFFFFFFF) == 0xFFFFFFFF);

		// UINT64_T
		CPPUNIT_ASSERT(reverse_bits((uint64_t)0x0000000000000000) == 0x0000000000000000);
		CPPUNIT_ASSERT(reverse_bits((uint64_t)0x0000000000100001) == 0x8000080000000000);
		CPPUNIT_ASSERT(reverse_bits((uint64_t)0x0100000000000400) == 0x0020000000000080);
		CPPUNIT_ASSERT(reverse_bits((uint64_t)0x0003000000000008) == 0x100000000000c000);
		CPPUNIT_ASSERT(reverse_bits((uint64_t)0xFEFEFEFEFEFEFEFE) == 0x7F7F7F7F7F7F7F7F);
		CPPUNIT_ASSERT(reverse_bits((uint64_t)0xFFFFFFFFFFFFFFFF) == 0xFFFFFFFFFFFFFFFF);
	}

	/* Test bit parity function */
	void test_bit_parity() {

		// UINT8_T
		CPPUNIT_ASSERT(bit_parity((uint8_t)0x00) == 0);
		CPPUNIT_ASSERT(bit_parity((uint8_t)0x01) == 1);
		CPPUNIT_ASSERT(bit_parity((uint8_t)0x03) == 0);
		CPPUNIT_ASSERT(bit_parity((uint8_t)0x07) == 1);
		CPPUNIT_ASSERT(bit_parity((uint8_t)0xFE) == 1);
		CPPUNIT_ASSERT(bit_parity((uint8_t)0xFF) == 0);

		// UINT16_T
		CPPUNIT_ASSERT(bit_parity((uint16_t)0x0000) == 0);
		CPPUNIT_ASSERT(bit_parity((uint16_t)0x0001) == 1);
		CPPUNIT_ASSERT(bit_parity((uint16_t)0x0003) == 0);
		CPPUNIT_ASSERT(bit_parity((uint16_t)0x0007) == 1);
		CPPUNIT_ASSERT(bit_parity((uint16_t)0x0080) == 1);
		CPPUNIT_ASSERT(bit_parity((uint16_t)0xFFFE) == 1);
		CPPUNIT_ASSERT(bit_parity((uint16_t)0xFFFF) == 0);

		// UINT32_T
		CPPUNIT_ASSERT(bit_parity((uint32_t)0x00000000) == 0);
		CPPUNIT_ASSERT(bit_parity((uint32_t)0x00108001) == 1);
		CPPUNIT_ASSERT(bit_parity((uint32_t)0x01000400) == 0);
		CPPUNIT_ASSERT(bit_parity((uint32_t)0x00030008) == 1);
		CPPUNIT_ASSERT(bit_parity((uint32_t)0xFFFEFFFF) == 1);
		CPPUNIT_ASSERT(bit_parity((uint32_t)0xFFFFFFFF) == 0);

		// UINT64_T
		CPPUNIT_ASSERT(bit_parity((uint64_t)0x0000000000000000) == 0);
		CPPUNIT_ASSERT(bit_parity((uint64_t)0x0080000000100001) == 1);
		CPPUNIT_ASSERT(bit_parity((uint64_t)0x0100002000000408) == 0);
		CPPUNIT_ASSERT(bit_parity((uint64_t)0x0003000000000008) == 1);
		CPPUNIT_ASSERT(bit_parity((uint64_t)0xFEFFFFFFFFFFFFFF) == 1);
		CPPUNIT_ASSERT(bit_parity((uint64_t)0xFFFFFFFFFFFFFFFF) == 0);
	}

	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("FrameTest");
		suite->addTest(new CppUnit::TestCaller<FrameTest>("Metadata", &FrameTest::testMetadata));
		suite->addTest(new CppUnit::TestCaller<FrameTest>("JSON parsing", &FrameTest::json_parsing_test));
		//suite->addTest(new CppUnit::TestCaller<FrameTest>("Bit operations", &FrameTest::test_bit_operations));
		suite->addTest(new CppUnit::TestCaller<FrameTest>("Bit Parity Test", &FrameTest::test_bit_parity));
		suite->addTest(new CppUnit::TestCaller<FrameTest>("Bit Reverse Test", &FrameTest::test_reverse_bits));
		return suite;
	}

};

#ifndef COMBINED_TEST
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(FrameTest::suite());
	runner.run();
	return 0;
}
#endif