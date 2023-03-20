#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <suo.hpp>
#include <modem/mod_psk.hpp>
#include <modem/demod_psk.hpp>
#include <framing/golay_framer.hpp>
#include <framing/golay_deframer.hpp>
#include <plotter.hpp>

#include "utils.hpp"

using namespace std;
using namespace suo;


#define MAX_SAMPLES 200000


class BPSKTest : public CppUnit::TestFixture
{
private:
	Timestamp now;

public:

	void setUp() {
		now = 0;// rand() & 0xFFFFFF;
	}


	void runTest()
	{		
		const float frequency_offset = 0.f;

		/* Contruct framer */
		GolayFramer::Config framer_conf;
		framer_conf.syncword = 0x1ACFFC1D;
		framer_conf.syncword_len = 32;
		framer_conf.preamble_len = 3 * 16 * 8;
		framer_conf.use_viterbi = false;
		framer_conf.use_randomizer = false;
		framer_conf.use_rs = false;

		GolayFramer framer(framer_conf);
		RandomFrameGenerator frame_generator(64);
		framer.sourceFrame.connect_member(&frame_generator, &RandomFrameGenerator::source_frame);


		/* Contruct modulator */
		PSKModulator::Config mod_conf;
		mod_conf.sample_rate = 50e3;
		mod_conf.symbol_rate = 9600;
		mod_conf.center_frequency = 0e3;
		mod_conf.ramp_up_duration = 3;
		mod_conf.ramp_down_duration = 3;

		PSKModulator mod(mod_conf);
		mod.generateSymbols.connect_member(&framer, &GolayFramer::generateSymbols);


		/* Construct deframer */
		GolayDeframer::Config deframer_conf;
		deframer_conf.syncword = framer_conf.syncword;
		deframer_conf.syncword_len = framer_conf.syncword_len;
		deframer_conf.sync_threshold = 3;
		deframer_conf.use_viterbi = framer_conf.use_viterbi;
		deframer_conf.use_randomizer = framer_conf.use_randomizer;
		deframer_conf.use_rs = framer_conf.use_rs;

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
		PSKDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.samples_per_symbol = 8;

		PSKDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &PSKDemodulator::lockReceiver);


		float SNRdB = 20;
		float signal_bandwidth = 2 * mod_conf.symbol_rate; // TODO:
		float noise_std = pow(10.0f, -SNRdB/20.0f); // Noise standard deviation
		noise_std /= (signal_bandwidth / mod_conf.sample_rate);

		SampleVector samples;
		samples.reserve(MAX_SAMPLES);

		// Feed some noise to demodulator
		generate_noise(samples, noise_std, 5000);
		demod.sinkSamples(samples, now);

		// Modulate the signal
		samples.clear();
		SampleGenerator sample_gen = mod.generateSamples(now);
		sample_gen.sourceSamples(samples);
		cout << "Generated " << samples.size() << " samples" << endl;
		CPPUNIT_ASSERT(samples.size() > 0);

		// Add imparities (noise and fractional delay)
		add_noise(samples, noise_std);
		delay_signal(randuf(0.0f, 1.0f), samples);

#if 1
		ComplexPlotter iq_plot(samples);
		iq_plot.iq.resize(1000);
		iq_plot.plot();
		//iq_plot.plot_constellation();
		matplot::show();
#endif

		// Demodulate the signal
		demod.sinkSamples(samples, now);

#if 1
		// Feed more noise to demodulator
		generate_noise(samples, noise_std, 100 + rand() % 1000);
		demod.sinkSamples(samples, now);
#endif

		// Assert the transmit and received frames
		CPPUNIT_ASSERT(received_frame.empty() == false);
		CPPUNIT_ASSERT(received_frame.size() == frame_generator.latest_frame().size());
		CPPUNIT_ASSERT(received_frame.data == frame_generator.latest_frame().data);

		Stats stats;
		count_bit_errors(stats, frame_generator.latest_frame(), received_frame);
		cout << stats;
	}


	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("BPSKTest");
		suite->addTest(new CppUnit::TestCaller<BPSKTest>("Basic test", &BPSKTest::runTest));
		return suite;
	}

};


#ifndef COMBINED_TEST
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(BPSKTest::suite());
	runner.run();
	return 0;
}
#endif
