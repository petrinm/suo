#include <iostream>
#include <ctime>
#include <cmath>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "modem/mod_gmsk.hpp"
#include "modem/demod_gmsk.hpp"
#include "modem/demod_gmsk_cont.hpp"
#include "modem/demod_fsk_mfilt.hpp"
#include "framing/golay_framer.hpp"
#include "framing/golay_deframer.hpp"

#include "utils.hpp"

using namespace std;
using namespace suo;



class EbNoTest : public CppUnit::TestFixture
{
private:
	Timestamp now;

	Frame transmit_frame;
	Frame received_frame;

public:

	void dummy_frame_sink(const Frame& frame, Timestamp _now) {
		(void)_now;
		received_frame = frame;
		cout << received_frame;
		//suo_frame_print(frame, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);
	}

	void source_dummy_frame(Frame& frame, Timestamp _now) {
#if 0
		static last = 0;
		if ((last - time) < 100)
			return 0;
		last = time;
#endif

		// Generate a new frame
		static unsigned int id = 1;
		size_t data_len = 200;
		frame.id = id++;
		frame.data.resize(data_len);
		//frame.data_len = 32 + rand() % 128;
		for (unsigned i = 0; i < data_len; i++)
			frame.data[i] = rand() % 256;

		//cout << frame;
		//suo_frame_print(frame, SUO_PRINT_DATA);

		// Copy the frame for later asserting
		transmit_frame = frame;
	}


	/* Sleep given amount of milliseconds */
	void sleep(unsigned int ms) {
		//usleep((ms) * 1000);
		now += (ms) * 1000;
	}

	void setUp() {
		srand(time(nullptr));
		now = time(nullptr) % 0xFFFFFF;
	}

	void tearDown() { }



	void runTest() {

#define MAX_SAMPLES 50000
		srand(time(NULL));

		SampleVector samples;
		samples.reserve(MAX_SAMPLES);

		float frequency_offset = 0e3;
		unsigned int symbol_rate_offset = 0;

		/* Contruct framer */
		GolayFramer::Config framer_conf;
		framer_conf.sync_word = 0xC9D08A7B;
		framer_conf.sync_len = 32;
		framer_conf.preamble_len = 2*64;
		framer_conf.use_viterbi = 0;
		framer_conf.use_randomizer = 1;
		framer_conf.use_rs = 0;

		GolayFramer framer(framer_conf);
		framer.sourceFrame.connect_member(this, &EbNoTest::source_dummy_frame);


		/* Contruct modulator */
		GMSKModulator::Config mod_conf;
		mod_conf.sample_rate = 50e3;
		mod_conf.symbol_rate = 9600;
		mod_conf.center_frequency = 10e3;
		mod_conf.bt = 0.5;

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
		deframer.sinkFrame.connect_member(this, &EbNoTest::dummy_frame_sink);

		/* Construct demodulator */
#if 0
		FSKMatchedFilterDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate + symbol_rate_offset;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.samples_per_symbol = 4;

		FSKMatchedFilterDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &FSKMatchedFilterDemodulator::lockReceiver);
#elif 1
		GMSKContinousDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate + symbol_rate_offset;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.samples_per_symbol = 4;

		GMSKContinousDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
#else
		GMSKDemodulator::Config demod_conf;
		demod_conf.sample_rate = mod_conf.sample_rate;
		demod_conf.symbol_rate = mod_conf.symbol_rate + symbol_rate_offset;
		demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
		demod_conf.samples_per_symbol = 4;
		demod_conf.sync_word = 0xC9D08A7B;
		demod_conf.sync_len = 32;

		GMSKDemodulator demod(demod_conf);
		demod.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
#endif


		float             SNRdB_min         = 6.0f;   // signal-to-noise ratio (minimum)
		float             SNRdB_max         = 16.0f;  // signal-to-noise ratio (maximum)
		unsigned int      num_snr           = 14;     // number of SNR steps
		unsigned int      num_packet_trials = 800;    // number of trials

		float signal_bandwidth = mod_conf.symbol_rate;
		float SNRdB_step = (SNRdB_max - SNRdB_min) / (num_snr - 1);


		for (unsigned int s = 0; s < num_snr; s++) {

			Timestamp time = 10000;

			float SNRdB = SNRdB_min + s*SNRdB_step; // SNR in dB for this round
			float nstd = powf(10.0f, -SNRdB/20.0f); // noise standard deviation
			nstd /= (signal_bandwidth / mod_conf.sample_rate);

			Stats stats;
			unsigned int total_errors = 0;
			unsigned int total_bits = 0;

			// for (unsigned int t=0; (t<num_packet_trials || total_errors < 20000); t++) {
			while (total_errors < 30000) {

				unsigned int noise_samples = MAX_SAMPLES/2  + rand() % (MAX_SAMPLES/2);
				samples.resize(noise_samples);
				for (unsigned i=0; i < noise_samples; i++)
					samples[i] = nstd * Complex(randnf(), randnf()) * M_SQRT1_2f;
				demod.sinkSamples(samples, time);

				// Modulate the signal
				samples.clear();
				mod.sourceSamples(samples, time);

#if 0
				cout << "\n\nOikea:\n";
				for (unsigned int i = 0; i < 3; i++) {
					unsigned int x = transmit_frame.data[i];
					for (int j = 0; j < 8; j++) {
						printf("%d ", (x & 0x80) != 0);
						x <<= 1;
					}
				}
				cout << endl;
#endif

				// Add imparities (noise)
				for (size_t i=0; i < samples.size(); i++)
					samples[i] += nstd * Complex(randnf(), randnf()) * M_SQRT1_2f;
				delay_signal((float)(rand() % 1000) / 1000, samples);

				// Demodulate the signal
				demod.sinkSamples(samples, time);

				// Assert the transmit and received frames
				if (received_frame.empty() == false) {
					count_bit_errors(stats, transmit_frame, received_frame);
					received_frame.clear();
				}
				else {
					stats.total_bit_errors += 8 * transmit_frame.size();
					stats.total_bits += 8 * transmit_frame.size();
				}

			}

			cout << "             SNR: " << SNRdB << " dB" << endl;
			cout << stats;
		}

	}

};



int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<EbNoTest>("EbNoTest", &EbNoTest::runTest));
	runner.run();
	return 0;
}
