#include <iostream>

#include <suo.hpp>
#include <modem/mod_gmsk.hpp>
#include <modem/demod_gmsk.hpp>
#include <modem/demod_gmsk_cont.hpp>
#include <framing/hdlc_framer.hpp>
#include <framing/hdlc_deframer.hpp>

#include "../utils.hpp"

using namespace std;
using namespace suo;


void gmsk_cont_hdlc_test() {

	srand(time(nullptr));
	Timestamp now = time(nullptr) % 0xFFFFFF;

	SampleVector samples;
	samples.reserve(80000);

	float frequency_offset = 0; // [Hz]
	unsigned int symbol_rate_offset = 0;

	/* Contruct framer */
	HDLCFramer::Config framer_conf;
	framer_conf.mode = HDLCMode::G3RUH;
	framer_conf.preamble_length = 10;
	framer_conf.trailer_length = 2;
	framer_conf.append_crc = false;

	HDLCFramer framer(framer_conf);
	RandomFrameGenerator frame_gen(64);
	framer.sourceFrame.connect_member(&frame_gen, &RandomFrameGenerator::source_frame);

	/* Contruct modulator */
	GMSKModulator::Config mod_conf;
	mod_conf.sample_rate = 50e3;
	mod_conf.symbol_rate = 9600;
	mod_conf.center_frequency = 10e3;
	mod_conf.bt = 0.5;
	mod_conf.ramp_up_duration = 3;
	mod_conf.ramp_down_duration = 3;

	GMSKModulator mod(mod_conf);
	mod.generateSymbols.connect_member(&framer, &HDLCFramer::generateSymbols);

	/* Construct deframer */
	HDLCDeframer::Config deframer_conf;
	deframer_conf.mode = framer_conf.mode;
	deframer_conf.check_crc = false;
	deframer_conf.maximum_frame_length = 256;
	deframer_conf.minimum_frame_length = 8;
	deframer_conf.minimum_silence = 5;

	HDLCDeframer deframer(deframer_conf);
	Frame received_frame;
	deframer.sinkFrame.connect([&](Frame& frame, Timestamp now) {
		(void)now;
		received_frame = frame;
		//cout << received_frame(Frame::PrintData | Frame::PrintMetadata | Frame::PrintColored);
	});

	/* Construct demodulator */
#if 1
	GMSKContinousDemodulator::Config demod_conf;
	demod_conf.sample_rate = mod_conf.sample_rate;
	demod_conf.symbol_rate = mod_conf.symbol_rate + symbol_rate_offset;
	demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
	demod_conf.samples_per_symbol = 8;

	GMSKContinousDemodulator demod(demod_conf);
	demod.sinkSymbol.connect_member(&deframer, &HDLCDeframer::sinkSymbol);
	deframer.syncDetected.connect_member(&demod, &GMSKContinousDemodulator::lockReceiver);
#else
	GMSKDemodulator::Config demod_conf;
	demod_conf.sample_rate = mod_conf.sample_rate;
	demod_conf.symbol_rate = mod_conf.symbol_rate + symbol_rate_offset;
	demod_conf.center_frequency = mod_conf.center_frequency + frequency_offset;
	demod_conf.samples_per_symbol = 4;
	demod_conf.syncword = 0xC9D08A7B;
	demod_conf.syncword_len = 32;

	GMSKDemodulator demod(demod_conf);
	demod.sinkSymbol.connect_member(&deframer, &HDLCDeframer::sinkSymbol);
	deframer.syncDetected.connect_member(&demod, &GMSKDemodulator::lockReceiver);
#endif


	float             SNRdB_min = 0.0f;   // signal-to-noise ratio (minimum)
	float             SNRdB_max = 20.0f;  // signal-to-noise ratio (maximum)
	unsigned int      num_snr = 21;       // number of SNR steps
	unsigned int      num_packet_trials = 800;	// number of trials

	float signal_bandwidth = mod_conf.sample_rate;
	float SNRdB_step = (SNRdB_max - SNRdB_min) / (num_snr - 1);

	SampleGenerator sample_gen;
	EbN0Log log("gmsk_cont_hdlc"); // "bt=%.1f", mod_conf.bt


	for (unsigned int s = 0; s < num_snr; s++) {

		Timestamp time = 10000;

		float SNRdB = SNRdB_min + s * SNRdB_step; // SNR in dB for this round
		float noise_std = powf(10.0f, -SNRdB / 20.0f); // noise standard deviation
		noise_std /= (signal_bandwidth / mod_conf.sample_rate);

		Stats stats;

		while (stats.total_bit_errors < 50000) {

			// Feed some noise to demodulator
			generate_noise(samples, noise_std, int(0.5 * mod_conf.sample_rate) + rand() % 1000);
			demod.sinkSamples(samples, time);

			// Modulate the signal
			samples.clear();
			sample_gen = mod.generateSamples(now);
			sample_gen.sourceSamples(samples);

			received_frame.clear();

			// Demodulate the signal
			add_noise(samples, noise_std);
			delay_signal(randuf(0.0f, 1.0f), samples);
			demod.sinkSamples(samples, time);

			ComplexPlotter iq_plot(samples);
			iq_plot.plot_fft();
			ComplexPlotter::show();

			// Feed more noise to demodulator
			generate_noise(samples, noise_std, 2000);
			demod.sinkSamples(samples, now);

			// Assert the transmit and received frames
			if (received_frame.empty() == false) {
				count_bit_errors(stats, frame_gen.latest_frame(), received_frame);
			}
			else {
				stats.total_bit_errors += 8 * frame_gen.latest_frame().size();
				stats.total_bits += 8 * frame_gen.latest_frame().size();
			}
			//cout << stats.total_bit_errors << " " << stats.total_bits << endl;
		}

		cout << endl;
		cout << "			 SNR: " << SNRdB << " dB" << endl;
		cout << stats;

		log.push_results(SNRdB, stats);
	}

}


#ifndef COMBINED_EBNO_TEST
int main(int argc, char** argv)
{
	gmsk_cont_hdlc_test();
	return 0;
}
#endif