#include <cppunit/ui/text/TestRunner.h>

#define COMBINED_TEST 1

//#include "coding/test_convolutional.cpp"
#include "coding/test_crc.cpp"
#include "coding/test_reed_solomon.cpp"

#include "test_golay_framing.cpp"
#include "test_hdlc_framing.cpp"

#include "test_generator.cpp"
#include "test_utils.cpp"


int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	
	// Utility tests
	runner.addTest(FrameTest::suite());
	runner.addTest(GeneratorTest::suite());

	// Coding tests
	//runner.addTest(ConvolutionalTest::suite());
	runner.addTest(CRCTest::suite());
	runner.addTest(ReedSolomonTest::suite());

	// Framing tests
	runner.addTest(GolayFramingTest::suite());
	runner.addTest(HDLCFramingTest::suite());


	runner.run();
	return 0;
}
