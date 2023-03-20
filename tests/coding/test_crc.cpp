#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <suo.hpp>
#include <coding/crc.hpp>
#include <coding/crc_generic.hpp>


using namespace std;
using namespace suo;


class CRCTest: public CppUnit::TestFixture
{
public:

	void run_reverse_bits_test()
	{
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint8_t)0x11), (uint8_t)0x88U);
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint8_t)0xF0), (uint8_t)0x0FU);
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint8_t)0x10), (uint8_t)0x08U);

		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint16_t)0x800FU), (uint16_t)0xF001U);
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint16_t)0xF0F0U), (uint16_t)0x0F0FU);
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint16_t)0x30F1U), (uint16_t)0x8F0CU);

		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint32_t)0x00000001U), (uint32_t)0x80000000U);
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint32_t)0x000000F0U), (uint32_t)0x0F000000U);
		CPPUNIT_ASSERT_EQUAL(reverse_bits((uint32_t)0x00000800U), (uint32_t)0x00100000U);
	}


	void run_crc16_test()
	{
		CRC16 crc(CRCAlgorithms::CRC16_X25);

		ByteVector data = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
		uint16_t calculated = crc.finalize(crc.update(crc.init(), data));
		CPPUNIT_ASSERT_EQUAL(calculated, (uint16_t)0x906E);
		
		// Test same but with calculate interface
		CPPUNIT_ASSERT_EQUAL(crc.calculate(data), (uint16_t)0x906E);

		// Test same but with digest interface
		CRC16::Digest digest(CRCAlgorithms::CRC16_AUG_CCITT);
		digest.update(data);
		calculated = digest.finalize();
		CPPUNIT_ASSERT_EQUAL(calculated, (uint16_t)0xE5CC);

		CRC16 crc_cdma(CRCAlgorithms::CRC16_CDMA2000);
		CPPUNIT_ASSERT_EQUAL(crc_cdma.calculate(data), (uint16_t)0x4C06);
	}

	void run_crc32_test()
	{
		CRC32 crc(CRCAlgorithms::CRC32);

		ByteVector data = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
		uint32_t calculated = crc.finalize(crc.update(crc.init(), data));
		CPPUNIT_ASSERT_EQUAL(0xCBF43926U, calculated);

		// Test same but with calculate interface
		CPPUNIT_ASSERT_EQUAL(0xCBF43926U, crc.calculate(data));

		// Test same but with digest interface
		CRC32::Digest digest(CRCAlgorithms::CRC32);
		digest.update(data);
		CPPUNIT_ASSERT_EQUAL(0xCBF43926U, digest.finalize());

		CRC32 crc_posix(CRCAlgorithms::CRC32_POSIX);
		CPPUNIT_ASSERT_EQUAL(0x765E7680U, crc_posix.calculate(data));
	}

	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("CRCTest");
		suite->addTest(new CppUnit::TestCaller<CRCTest>("reverse_bits", &CRCTest::run_reverse_bits_test));
		//suite->addTest(new CppUnit::TestCaller<CRCTest>("CRC-8", &CRCTest::run_crc8_test));
		suite->addTest(new CppUnit::TestCaller<CRCTest>("CRC-16", &CRCTest::run_crc16_test));
		suite->addTest(new CppUnit::TestCaller<CRCTest>("CRC-32", &CRCTest::run_crc32_test));
		return suite;
	}
	
};


#ifndef COMBINED_TEST
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(CRCTest::suite());
	runner.run();
	return 0;
}
#endif
