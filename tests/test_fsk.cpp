#include <iostream>
#include <cmath>
#include <ctime>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <matplot/matplot.h>

#include "suo.hpp"
#include "modem/mod_fsk.hpp"
#include "modem/demod_fsk_mfilt.hpp"
#include "framing/hdlc_framer.hpp"
#include "framing/hdlc_deframer.hpp"

#include "plotter.hpp"
#include "utils.hpp"

using namespace std;
using namespace suo;


#define MAX_SAMPLES 200000


class FSKTest : public CppUnit::TestFixture
{
private:
	Timestamp now;
	Stats stats;
	SampleVector samples;
	Frame transmit_frame;
	Frame received_frame;
	bool frame_generated;

public:

	void setUp() {
		srand(time(nullptr));
		now = 0;// rand() & 0xFFFFFF;

		samples.reserve(MAX_SAMPLES);
		samples.resize(MAX_SAMPLES);
		samples.clear();
		transmit_frame.clear();
		received_frame.clear();
		frame_generated = false;
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
		if (frame_generated) return;
		frame_generated = true;

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
		HDLCFramer::Config framer_conf;
		framer_conf.mode = HDLCFramer::G3RUH;
		framer_conf.preamble_length = 4;
		framer_conf.trailer_length = 2;
		framer_conf.append_crc = false;
		HDLCFramer framer(framer_conf);
		framer.sourceFrame.connect_member(this, &FSKTest::source_dummy_frame);


		/* Contruct modulator */
		FSKModulator::Config mod_conf;
		mod_conf.sample_rate = 50e3;
		mod_conf.symbol_rate = 9600;
		mod_conf.center_frequency = 0e3;
		mod_conf.modindex = 1.0f;
		mod_conf.bt = 0.9;
		mod_conf.ramp_up_duration = 3;
		mod_conf.ramp_down_duration = 3;

		FSKModulator mod(mod_conf);
		mod.sourceSymbols.connect_member(&framer, &HDLCFramer::sourceSymbols);


		/* Construct deframer */
		HDLCDeframer::Config deframer_conf;
		deframer_conf.mode = HDLCFramer::G3RUH;
		deframer_conf.check_crc = false;
		deframer_conf.maximum_frame_length = 256;
		deframer_conf.minimum_frame_length = 8;
		deframer_conf.minimum_silence = 5;

		HDLCDeframer deframer(deframer_conf);
		deframer.sinkFrame.connect_member(this, &FSKTest::dummy_frame_sink);
		deframer.syncDetected.connect_member(this, &FSKTest::dummy_sync_detected);


		/* Construct demodulator */
		FSKMatchedFilterDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.samples_per_symbol = 8;
		demod_conf.bt = mod_conf.bt;

		FSKMatchedFilterDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &HDLCDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &FSKMatchedFilterDemodulator::lockReceiver);


		float SNRdB = 20;
		float signal_bandwidth = 2 * mod_conf.symbol_rate * mod_conf.modindex; // TODO:
		float noise_std = pow(10.0f, -SNRdB/20.0f); // Noise standard deviation
		noise_std /= (signal_bandwidth / mod_conf.sample_rate);

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
		CPPUNIT_ASSERT(received_frame.data.size() == transmit_frame.data.size());
		CPPUNIT_ASSERT(transmit_frame.data == received_frame.data);


		count_bit_errors(stats, transmit_frame, received_frame);
		cout << stats;
	}

};



int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<FSKTest>("FSKTest", &FSKTest::runTest));
	runner.run();
	return 0;
}
