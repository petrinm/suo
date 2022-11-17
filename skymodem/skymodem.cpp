#include <string>
#include <iostream>

#include "suo.hpp"
#include "signal-io/soapysdr_io.hpp"
#include "modem/demod_fsk_mfilt.hpp"
#include "modem/demod_gmsk_cont.hpp"
#include "modem/mod_gmsk.hpp"
#include "frame-io/zmq_interface.hpp"
#include "framing/golay_framer.hpp"
#include "framing/golay_deframer.hpp"


using namespace std;
using namespace suo;


#define ZMQ_URI_LEN 64



int main(int argc, char *argv[])
{

	int modem_base = 4000;
	if (argc >= 2)
		modem_base = atoi(argv[1]);

	try {
		/*
		* SDR
		*/
		SoapySDRIO::Config sdr_conf;
		sdr_conf.rx_on = true;
		sdr_conf.tx_on = true;
		sdr_conf.use_time = 1;
		sdr_conf.samplerate = 500000;
		sdr_conf.tx_latency = 100; // [samples]

		//sdr_conf.buffer = 1024;
		sdr_conf.buffer = (sdr_conf.samplerate / 1000); // buffer lenght in milliseconds

		sdr_conf.args["device"] = "uhd";

		sdr_conf.rx_centerfreq = 437.00e6;
		sdr_conf.tx_centerfreq = 437.00e6;

		sdr_conf.rx_gain = 30;
		sdr_conf.tx_gain = 60;

		sdr_conf.rx_antenna = "TX/RX";
		sdr_conf.tx_antenna = "TX/RX";

		SoapySDRIO sdr(sdr_conf);


		/*
		* Setup receiver
		*/
#if 0
		FSKMatchedFilterDemodulator::Config receiver_conf;
		receiver_conf.sample_rate = sdr_conf.samplerate;
		receiver_conf.center_frequency = 125.0e3;
#else
		GMSKContinousDemodulator::Config receiver_conf;
#endif
		/*
		* Setup frame decoder
		*/
		GolayDeframer::Config deframer_conf;
		deframer_conf.sync_word = 0x1ACFFC1D;
		deframer_conf.sync_len = 32;
		deframer_conf.sync_threshold = 3;
		deframer_conf.skip_rs = 1;
		deframer_conf.skip_randomizer = 1;
		deframer_conf.skip_viterbi = 1;

#if 0
		/* For 9600 baud */
		receiver_conf.symbol_rate = 9600;
		FSKMatchedFilterDemodulator receiver_9k6(receiver_conf);
		GolayDeframer deframer_9k6(deframer_conf);
		receiver_9k6.sinkSymbol.connect_member(&deframer_9k6, &GolayDeframer::sinkSymbol);
		sdr.sinkSamples.connect_member(&receiver_9k6, &FSKMatchedFilterDemodulator::sinkSamples);
#else
		receiver_conf.symbol_rate = 9600;
		GMSKContinousDemodulator receiver_9k6(receiver_conf);
		GolayDeframer deframer_9k6(deframer_conf);
		receiver_9k6.sinkSymbol.connect_member(&deframer_9k6, &GolayDeframer::sinkSymbol);
		deframer_9k6.syncDetected.connect_member(&receiver_9k6, &GMSKContinousDemodulator::lockReceiver);
		sdr.sinkSamples.connect_member(&receiver_9k6, &GMSKContinousDemodulator::sinkSamples);
#endif

#if 0
		/* For 19200 baud */
		receiver_conf.symbol_rate = 19200;
		FSKMatchedFilterDemodulator receiver_19k2(receiver_conf);
		GolayDeframer deframer_19k2(deframer_conf);
		receiver_19k62.sinkSymbol.connect_member(&deframer_19k2, &GolayDeframer::sinkSymbol);
		deframer_19k62.syncDetected.connect_member(&receiver_19k2, &GMSKContinousDemodulator::lockReceiver);
		sdr.sinkSamples.connect_member(&receiver_19k2, &FSKMatchedFilterDemodulator::sinkSamples);

		/* For 36400 baud */
		receiver_conf.symbol_rate = 36400;
		FSKMatchedFilterDemodulator receiver_36k4(receiver_conf);
		GolayDeframer deframer_36k4(deframer_conf);
		receiver_36k4.sinkSymbol.connect_member(&deframer_36k4, &GolayDeframer::sinkSymbol);
		deframer_36k4.syncDetected.connect_member(&receiver_19k2, &GMSKContinousDemodulator::lockReceiver);
		sdr.sinkSamples.connect_member(&receiver_36k4, &FSKMatchedFilterDemodulator::sinkSamples);
#endif

#if 0
		void lockReceivers(bool locked, Timestamp now) {
			receiver_9k6.lockReceivers(locked, now);
			receiver_19k2.lockReceivers(locked, now);
			receiver_36k4.lockReceivers(locked, now);
		}

		deframer_9k6.syncDetected.connect(lockReceivers);
		deframer_19k2.syncDetected.connect(lockReceivers);
		deframer_36k4.syncDetected.connect(lockReceivers);
#endif

		/*
		* ZMQ output
		*/
		ZMQOutput::Config zmq_output_conf;
		zmq_output_conf.address = "tcp://127.0.0.1:" + to_string(modem_base);
		zmq_output_conf.address_tick = "tcp://127.0.0.1:" + to_string(modem_base + 2);
		zmq_output_conf.binding = 1;
		zmq_output_conf.binding_ticks =  1;
		zmq_output_conf.thread = 1;

		ZMQOutput zmq_output(zmq_output_conf);
		deframer_9k6.sinkFrame.connect_member(&zmq_output, &ZMQOutput::sinkFrame);
		//deframer_19k2.sinkFrame.connect_member(&zmq_output, &ZMQOutput::sinkFrame);
		//deframer_36k4.sinkFrame.connect_member(&zmq_output, &ZMQOutput::sinkFrame);

		sdr.sinkTicks.connect_member(&zmq_output, &ZMQOutput::tick);

		/*
		* Setup transmitter
		*/
		GMSKModulator::Config modulator_conf;
		modulator_conf.sample_rate = sdr_conf.samplerate;
		modulator_conf.symbol_rate = 9600;
		modulator_conf.center_frequency = 125.0e3;
		modulator_conf.bt = 0.5;
		modulator_conf.ramp_up_duration = 2;
		modulator_conf.ramp_down_duration = 2;

		GMSKModulator modulator(modulator_conf);

		/*
		* Setup framer
		*/
		GolayFramer::Config framer_conf;
		framer_conf.sync_word = 0x1ACFFC1D;
		framer_conf.sync_len = 32;
		framer_conf.preamble_len = 12 * 8;
		framer_conf.use_viterbi = 0;
		framer_conf.use_randomizer = 0;
		framer_conf.use_rs = 0;
		framer_conf.golay_flags = 0x600; // Force RS flag on

		GolayFramer framer(framer_conf);

		/*
		* ZMQ input
		*/
		ZMQInput::Config zmq_input_conf;
		zmq_input_conf.address = "tcp://127.0.0.1:" + to_string(modem_base + 1);
		zmq_input_conf.address_tick = "tcp://127.0.0.1:" + to_string(modem_base + 3);
		zmq_input_conf.binding = true;
		zmq_input_conf.binding_ticks = 1;
		//zmq_input_conf.thread = 1;

		ZMQInput zmq_input(zmq_input_conf);

		framer.sourceFrame.connect_member(&zmq_input, &ZMQInput::sourceFrame);
		modulator.sourceSymbols.connect_member(&framer, &GolayFramer::sourceSymbols);
		sdr.sourceSamples.connect_member(&modulator, &GMSKModulator::sourceSamples);

		//zmq_input->set_frame_sink(zmq_input, transmitter, transmitter);
		//framer->set_frame_source(framer, zmq_input, zmq_input);


		zmq_input.reset();

		/*
		* Run!
		*/
		sdr.execute();
		cerr << "Suo exited" << endl;
		return 0;
	}
	catch (const SuoError& e) {
		cerr << "SuoError: " << e.what() << endl;
		return 1;
	}

}
