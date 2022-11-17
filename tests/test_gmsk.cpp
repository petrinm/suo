#include <iostream>
#include <cmath>
#include <ctime>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <matplot/matplot.h>

#include "suo.hpp"
#include "modem/mod_gmsk.hpp"
#include "modem/demod_gmsk.hpp"
#include "modem/demod_gmsk_cont.hpp"
#include "modem/demod_fsk_mfilt.hpp"
#include "framing/golay_framer.hpp"
#include "framing/golay_deframer.hpp"

#include "plotter.hpp"
#include "utils.hpp"

using namespace std;
using namespace suo;


#define MAX_SAMPLES 200000

class GMSKTest : public CppUnit::TestFixture
{
private:
	Timestamp now;
	Stats stats;
	SampleVector samples;
	Frame transmit_frame;
	Frame received_frame;

public:

	void setUp() {
		srand(time(nullptr));
		now = 0;// rand() & 0xFFFFFF;

		samples.reserve(MAX_SAMPLES);
		samples.resize(MAX_SAMPLES);
		samples.clear();
		transmit_frame.clear();
		received_frame.clear();
		stats.clear();
	}


	void dummy_frame_sink(const Frame &frame, Timestamp _now) {
		(void)_now;
		received_frame = frame;
		cout << "Received:" << endl;
		cout << frame(Frame::PrintData | Frame::PrintMetadata | Frame::PrintAltColor | Frame::PrintColored);
	}


	void source_dummy_frame(Frame &frame, Timestamp _now) {
		(void)_now;
#if 0
		static last = 0;
		if ((last - time) < 100)
			return 0;
		last = time;
#endif

		// Generate a new frame
		size_t data_len = 200;
		static unsigned int id = 1;
		frame.id = id++;
		frame.timestamp = _now;
		frame.data.resize(data_len);
		//frame.data_len = 32 + rand() % 128;
		for (unsigned i = 0; i < data_len; i++)
			frame.data[i] = rand() % 256;

		cout << frame(Frame::PrintData | Frame::PrintColored);

		// Copy the frame for later asserting
		transmit_frame = frame; 
	}


	void dummy_sync_detected(bool sync, Timestamp _now) {
		(void)_now;
		cout << "Sync detected = " << (sync ? "true" : "false") << endl;
	}


	void runTest()
	{		
		const float frequency_offset = 0.f;

		/* Contruct framer */
		GolayFramer::Config framer_conf;
		framer_conf.sync_word = 0xC9D08A7B;
		framer_conf.sync_len = 32;
		framer_conf.preamble_len = 64;
		framer_conf.use_viterbi = 0;
		framer_conf.use_randomizer = 0;
		framer_conf.use_rs = 0;

		GolayFramer framer(framer_conf);
		framer.sourceFrame.connect_member(this, &GMSKTest::source_dummy_frame);


		/* Contruct modulator */
		GMSKModulator::Config mod_conf;
		mod_conf.sample_rate = 50e3;
		mod_conf.symbol_rate = 9600;
		mod_conf.center_frequency = 0e3;
		mod_conf.bt = 0.9;
		mod_conf.ramp_up_duration = 3;
		mod_conf.ramp_down_duration = 3;

		GMSKModulator mod(mod_conf);
		mod.sourceSymbols.connect_member(&framer, &GolayFramer::sourceSymbols);


		/* Construct deframer */
		GolayDeframer::Config deframer_conf;
		deframer_conf.sync_word = framer_conf.sync_word;
		deframer_conf.sync_len = framer_conf.sync_len;
		deframer_conf.sync_threshold = 4;
		deframer_conf.skip_viterbi = 1;
		deframer_conf.skip_randomizer = 0;
		deframer_conf.skip_rs = 1;

		GolayDeframer deframer(deframer_conf);
		deframer.sinkFrame.connect_member(this, &GMSKTest::dummy_frame_sink);
		deframer.syncDetected.connect_member(this, &GMSKTest::dummy_sync_detected);


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
#elif 1
		GMSKContinousDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.bt = mod_conf.bt;

		GMSKContinousDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
#else
		GMSKDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.sync_word = framer_conf.sync_word;
		demod_conf.sync_len = framer_conf.sync_len;

		GMSKDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
#endif

		float SNRdB = 20;
		float noise_std = pow(10.0f, -SNRdB/20.0f); // Noise standard deviation

		// Feed some noise to demodulator
		generate_noise(samples, noise_std, 5000);
		demod.sinkSamples(samples, now);

		// Modulate the signal
		samples.clear();
		mod.sourceSamples(samples, now);
		cout << "Generated " << samples.size() << " samples" << endl;
		CPPUNIT_ASSERT(samples.size() > 0);

		// Add imparities (noise and fractional delay)
		add_noise(samples, noise_std);
		delay_signal(randuf(0.0f, 1.0f), samples);

#if 0
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
		CPPUNIT_ASSERT(received_frame.data.size() == transmit_frame.data.size());
		CPPUNIT_ASSERT(transmit_frame.data == received_frame.data);


		count_bit_errors(stats, transmit_frame, received_frame);
		cout << stats;
	}

};



int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<GMSKTest>("GMSKTest", &GMSKTest::runTest));
	runner.run();
	return 0;
}
