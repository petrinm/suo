#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "modem/mod_gmsk.hpp"
#include "modem/demod_gmsk.hpp"

#include <liquid/liquid.h>

using namespace std;
using namespace suo;



class GMSKTest : public CppUnit::TestFixture
{
private:
	Timestamp now;
	const Frame received_frame;
	bool frame_txed;

public:

	void setUp() {
		srand(time(nullptr));
		now = 0; // time(nullptr) % 0xFFFFFF;

		//symbols.reserve(1024);
		//symbols.clear();
		//transmit_frame = NULL;
		frame_txed = false;
	}

	void tearDown() { }


	void source_dummy_symbols(SymbolVector& symbols, Timestamp _now)
	{
		(void)_now;

		if (frame_txed) return;
		frame_txed = true;

		Symbol* ptr = &symbols[0];

		for (int i = 0; i < 32; i++)
			*ptr++ = (i & 1);

		*ptr++ = 0;
		*ptr++ = 1;
		*ptr++ = 1;
		*ptr++ = 1;

		*ptr++ = 1;
		*ptr++ = 0;
		*ptr++ = 1;
		*ptr++ = 0;

		symbols.resize(32 + 8);
	}


	void source_dummy_frame(Frame &frame, Timestamp _now) {
		(void)_now;
		frame.id = 2;
		frame.timestamp = 0;
		frame.data.resize(28);
		for (size_t i = 0; i < 28; i++)
			frame.data[i] = i;
	}


	void test_gmsk()
	{
#if 0
		BitVector bits;
		bits.reserve(1024);

		// frame detector
		unsigned preamble_len = 63;
		msequence ms = msequence_create(6, 0x6d, 1);

		cout << endl << endl;
		for (int i=0; i < preamble_len + 6; i++) {
			unsigned char bit = msequence_advance(ms);
			cout << bit << endl;
		}
		cout << endl << endl;
#endif

#if 0
		unsigned int k = 4; // filter samples/symbol
		unsigned int m = 3; // filter semi-length (symbols)
		float bt = 0.3; // filter bandwidth-time product
		unsigned int npfb = 32; // number of filters in symsync

		firpfb_rrrf mf = firpfb_rrrf_create_rnyquist(LIQUID_FIRFILT_GMSKRX, npfb, k, m, bt);
		firpfb_rrrf dmf = firpfb_rrrf_create_drnyquist(LIQUID_FIRFILT_GMSKRX, npfb, k, m, bt);

		firpfb_rrrf_print(mf);
		firpfb_rrrf_print(dmf);

#endif


#if 1
#define MAX_SAMPLES 500

		SampleVector samples;
		samples.reserve(MAX_SAMPLES);

		GMSKModulator::Config gmsk_conf;
		gmsk_conf.sample_rate = 50e3;
		gmsk_conf.symbol_rate = 9600;
		gmsk_conf.center_frequency = 10e3;
		gmsk_conf.bt = 0.3;

		GMSKModulator gmsk_mod(gmsk_conf);
		gmsk_mod.sourceSymbols.connect_member(this, &GMSKTest::source_dummy_symbols);

		unsigned int total_samples = 0;
		for (int i = 0; i < 1000; i++) {

			samples.clear();
			gmsk_mod.sourceSamples(samples, now);
			cout << "Generated " << samples.size() << " new samples" << endl;
			if (samples.size() == 0)
				break;

			total_samples += samples.size();
			now += 1000;
		}
		cout << "total samples = " << total_samples << endl;
		CPPUNIT_ASSERT(total_samples > 0);

		// TODO: Change frequency
		//mod->setFrequencyOffset(800.0f);
#endif
	}

	static CppUnit::TestSuite* suite() {
		CppUnit::TestSuite* gsmk_suite = new CppUnit::TestSuite;
		gsmk_suite->addTest(new CppUnit::TestCaller<GMSKTest>("GMSKTest", &GMSKTest::test_gmsk));
		return gsmk_suite;
	}

};


#if 1
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(GMSKTest::suite());
	runner.run();
	return 0;
}
#endif