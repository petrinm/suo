#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"

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

	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("FrameTest");
		suite->addTest(new CppUnit::TestCaller<FrameTest>("Metadata", &FrameTest::testMetadata));
		suite->addTest(new CppUnit::TestCaller<FrameTest>("JSON parsing", &FrameTest::json_parsing_test));
		//suite->addTest(new CppUnit::TestCaller<FrameTest>("Bit operations", &CRCTest::test_bit_operations));
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