#include <iostream>
#include <cmath>
#include <ctime>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <suo.hpp>
#include <modem/mod_gmsk.hpp>
#include <modem/demod_gmsk.hpp>
#include <modem/demod_gmsk_cont.hpp>
#include <modem/demod_fsk_mfilt.hpp>
#include <framing/golay_framer.hpp>
#include <framing/golay_deframer.hpp>
#include <plotter.hpp>

#include "utils.hpp"

using namespace std;
using namespace suo;


#define MAX_SAMPLES 200000

class GMSKTest : public CppUnit::TestFixture
{
private:
	Timestamp now;


public:

	void setUp() {
		srand(time(nullptr));
		now = 0;// rand() & 0xFFFFFF;
	}

	void runTest()
	{		
		const float frequency_offset = 0.f;

		/* Contruct framer */
		GolayFramer::Config framer_conf;
		framer_conf.syncword = 0xC9D08A7B;
		framer_conf.syncword_len = 32;
		framer_conf.preamble_len = 16*8;
		framer_conf.use_viterbi = false;
		framer_conf.use_randomizer = true;
		framer_conf.use_rs = true;

		GolayFramer framer(framer_conf);
		RandomFrameGenerator frame_generator(28);
		framer.sourceFrame.connect_member(&frame_generator, &RandomFrameGenerator::source_frame);


		/* Contruct modulator */
		GMSKModulator::Config mod_conf;
		mod_conf.sample_rate = 50e3;
		mod_conf.symbol_rate = 9600;
		mod_conf.center_frequency = 0;
		mod_conf.bt = 0.5;
		mod_conf.ramp_up_duration = 8;
		mod_conf.ramp_down_duration = 8;

		GMSKModulator mod(mod_conf);
		mod.generateSymbols.connect_member(&framer, &GolayFramer::generateSymbols);


		/* Construct deframer */
		GolayDeframer::Config deframer_conf;
		deframer_conf.syncword = framer_conf.syncword;
		deframer_conf.syncword_len = framer_conf.syncword_len;
		deframer_conf.sync_threshold = 4;

		GolayDeframer deframer(deframer_conf);
		Frame received_frame;
		deframer.sinkFrame.connect([&](Frame& frame, Timestamp now) {
			(void)now;
			received_frame = frame;
			cout << frame(Frame::PrintData | Frame::PrintMetadata | Frame::PrintAltColor | Frame::PrintColored);
		});

		deframer.syncDetected.connect([&](bool sync, Timestamp now) {
			(void)now;
			cout << "Sync detected = " << (sync ? "true" : "false") << endl;
		});


		/* Construct demodulator */
#if 0
		FSKMatchedFilterDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.samples_per_symbol = 8;
		demod_conf.bt = mod_conf.bt;

		FSKMatchedFilterDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &FSKMatchedFilterDemodulator::lockReceiver);
		demod.setMetadata.connect_member(&deframer, &GolayDeframer::setMetadata);
#elif 1
		GMSKContinousDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.bt = mod_conf.bt;
		demod_conf.samples_per_symbol = 4;

		GMSKContinousDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
		demod.setMetadata.connect_member(&deframer, &GolayDeframer::setMetadata);
#else
		GMSKDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.syncword = framer_conf.syncword;
		demod_conf.syncword_len = framer_conf.syncword_len;

		GMSKDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
		demod.setMetadata.connect_member(&deframer, &GolayDeframer::setMetadata);
#endif


		float SNRdB = 35;
		float signal_bandwidth = mod_conf.symbol_rate;
		float noise_std = pow(10.0f, -SNRdB/20.0f); // Noise standard deviation
		noise_std /= (signal_bandwidth / mod_conf.sample_rate);

		SampleVector samples;
		samples.reserve(MAX_SAMPLES);

		// Feed some noise to demodulator
		generate_noise(samples, noise_std, 500);
		demod.sinkSamples(samples, now);

		// Modulate the signal
		samples.clear();
		SampleGenerator sample_gen = mod.generateSamples(now);
		CPPUNIT_ASSERT(sample_gen.running() == true);
		sample_gen.sourceSamples(samples);
		cout << "Generated " << samples.size() << " samples" << endl;
		CPPUNIT_ASSERT(samples.size() > 0);

		// Add imparities (noise and fractional delay)
		add_noise(samples, noise_std);
		delay_signal(randuf(0.0f, 1.0f), samples);

#ifdef SUO_SUPPORT_PLOTTING
		ComplexPlotter iq_plot(samples);
		iq_plot.iq.resize(1000);
		iq_plot.plot();
		iq_plot.plot_constellation();
		matplot::show();
#endif

		// Demodulate the signal
		demod.sinkSamples(samples, now);

#if 1
		// Feed more noise to demodulator
		generate_noise(samples, noise_std, 100 + rand() % 1000);
		demod.sinkSamples(samples, now);
#endif
		
		Stats stats;
		count_bit_errors(stats, frame_generator.latest_frame(), received_frame);
		cout << stats;

		// Assert the transmit and received frames
		CPPUNIT_ASSERT(received_frame.empty() == false);
		CPPUNIT_ASSERT(received_frame.size() == frame_generator.latest_frame().size());
		CPPUNIT_ASSERT(received_frame.data == frame_generator.latest_frame().data);
	}



	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("GMSKTest");
		suite->addTest(new CppUnit::TestCaller<GMSKTest>("Basic test", &GMSKTest::runTest));
		return suite;
	}


};


#ifndef COMBINED_TEST
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(GMSKTest::suite());
	runner.run();
	return 0;
}
#endif