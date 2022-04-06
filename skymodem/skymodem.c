#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>

#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "suo.h"
#include "mux.h"
#include "signal-io/soapysdr_io.h"
#include "modem/demod_fsk_mfilt.h"
#include "modem/mod_gmsk.h"
#include "frame-io/zmq_interface.h"
#include "framing/golay_framer.h"
#include "framing/golay_deframer.h"


#define ZMQ_URI_LEN 64

int main(int argc, char *argv[])
{

	int modem_base = 4000;
	if (argc >= 2)
		modem_base = atoi(argv[1]);

	/*
	 * SDR
	 */
	const struct signal_io_code* sdr = &soapysdr_io_code;
	struct soapysdr_io_conf* sdr_conf = sdr->init_conf();

	sdr_conf->rx_on = 1;
	sdr_conf->tx_on = 1;
	sdr_conf->use_time = 1;
	sdr_conf->samplerate = 500000;
	sdr_conf->tx_latency = 100; // [samples]

	//sdr_conf->buffer = 1024;
	sdr_conf->buffer = (sdr_conf->samplerate / 1000); // buffer lenght in milliseconds

	sdr->set_conf(sdr_conf, "soapy-driver", "uhd");

	sdr_conf->rx_centerfreq = 437.00e6;
	sdr_conf->tx_centerfreq = 437.00e6;

	sdr_conf->rx_gain = 30;
	sdr_conf->tx_gain = 60;

	sdr_conf->rx_antenna = "TX/RX";
	sdr_conf->tx_antenna = "TX/RX";


	void* sdr_inst = sdr->init(sdr_conf);
	assert(sdr_inst);


	/*
	 * Setup receiver
	 */
	const struct receiver_code *receiver = &demod_fsk_mfilt_code;
	struct fsk_demod_mfilt_conf* receiver_conf = (struct fsk_demod_mfilt_conf*)receiver->init_conf();
	receiver_conf->sample_rate = sdr_conf->samplerate;
	receiver_conf->center_frequency = 125.0e3;

	/*
	 * Setup frame decoder
	 */
	const struct decoder_code *deframer = &golay_deframer_code;
	struct golay_deframer_conf* deframer_conf = (struct golay_deframer_conf*)deframer->init_conf();

	deframer_conf->sync_word = 0x1ACFFC1D;
	deframer_conf->sync_len = 32;
	deframer_conf->sync_threshold = 3;
	deframer_conf->skip_rs = 1;
	deframer_conf->skip_randomizer = 1;
	deframer_conf->skip_viterbi = 1;


	/* For 9600 baud */
	receiver_conf->symbol_rate = 9600;
	void* receiver_9k6_inst = receiver->init(receiver_conf);
	assert(receiver_9k6_inst);
	void* deframer_9k6_inst = deframer->init(deframer_conf);
	assert(deframer_9k6_inst);
	receiver->set_symbol_sink(receiver_9k6_inst, deframer->sink_symbol, deframer_9k6_inst);

	/* For 19200 baud */
	receiver_conf->symbol_rate = 19200;
	void* receiver_19k2_inst = receiver->init(receiver_conf);
	assert(receiver_19k2_inst);
	void* deframer_19k2_inst = deframer->init(deframer_conf);
	assert(deframer_19k2_inst);
	receiver->set_symbol_sink(receiver_19k2_inst, deframer->sink_symbol, deframer_19k2_inst);

	/* For 36400 baud */
	receiver_conf->symbol_rate = 36400;
	void* receiver_36k4_inst = receiver->init(receiver_conf);
	assert(receiver_36k4_inst);
	void* deframer_36k4_inst = deframer->init(deframer_conf);
	assert(deframer_36k4_inst);
	receiver->set_symbol_sink(receiver_36k4_inst, deframer->sink_symbol, deframer_36k4_inst);


#if 0
	sdr->set_sample_sink(sdr_inst, receiver->sink_samples, receiver_9k6_inst);
#else
	/*
	 * Create sample mux
	 */
	const struct sample_mux_code *sample_mux = &sample_sink_mux_code;
	void* sample_mux_inst = sample_mux->init(NULL); /* No configurations */
	sample_mux->set_sample_sink(sample_mux_inst, receiver->sink_samples, receiver_9k6_inst);
	sample_mux->set_sample_sink(sample_mux_inst, receiver->sink_samples, receiver_19k2_inst);
	sample_mux->set_sample_sink(sample_mux_inst, receiver->sink_samples, receiver_36k4_inst);
	sdr->set_sample_sink(sdr_inst, sample_mux->sink_samples, sample_mux_inst);
#endif

	/*
	 * ZMQ output
	 */
	const struct rx_output_code *zmq_output = &zmq_rx_output_code;
	struct zmq_rx_output_conf* zmq_output_conf = (struct zmq_rx_output_conf*)zmq_output->init_conf();
	zmq_output_conf->address = calloc(ZMQ_URI_LEN, sizeof(char));
	zmq_output_conf->address_tick = calloc(ZMQ_URI_LEN, sizeof(char));
	snprintf(zmq_output_conf->address, ZMQ_URI_LEN, "tcp://127.0.0.1:%d", modem_base);
	snprintf(zmq_output_conf->address_tick, ZMQ_URI_LEN, "tcp://127.0.0.1:%d", modem_base + 2);
	zmq_output_conf->binding = 1;
	zmq_output_conf->binding_ticks =  1;
	zmq_output_conf->thread = 1;

	void* zmq_output_inst = zmq_output->init(zmq_output_conf);
	assert(zmq_output_inst);

	deframer->set_frame_sink(deframer_9k6_inst, zmq_output->sink_frame, zmq_output_inst);
	deframer->set_frame_sink(deframer_19k2_inst, zmq_output->sink_frame, zmq_output_inst);
	deframer->set_frame_sink(deframer_36k4_inst, zmq_output->sink_frame, zmq_output_inst);
	sdr->set_tick_sink(sdr_inst, zmq_output->sink_tick, zmq_output_inst);

	/*
	 * Setup transmitter
	 */
	const struct transmitter_code *transmitter = &mod_gmsk_code;
	struct mod_gmsk_conf* transmitter_conf = (struct mod_gmsk_conf*)transmitter->init_conf();
	transmitter_conf->samplerate = sdr_conf->samplerate;
	transmitter_conf->symbolrate = 9600;
	transmitter_conf->centerfreq = 125.0e3;
	transmitter_conf->bt = 0.5;
	void* transmitter_inst = transmitter->init(transmitter_conf);
	assert(transmitter_inst);

	/*
	 * Setup framer
	 */
	const struct encoder_code *framer = &golay_framer_code;
	struct golay_framer_conf* framer_conf = (struct golay_framer_conf*)framer->init_conf();
	framer_conf->sync_word = 0x1ACFFC1D;
	framer_conf->sync_len = 32;
	framer_conf->preamble_len = 64;
	framer_conf->use_viterbi = 0;
	framer_conf->use_randomizer = 0;
	framer_conf->use_rs = 0;
	framer_conf->tail_length = 16;
	framer_conf->golay_flags = 0x600; // Force RS flag on
	void* framer_inst = framer->init(framer_conf);
	assert(framer_inst);

	/*
	 * ZMQ input
	 */
	const struct tx_input_code *zmq_input = &zmq_tx_input_code;
	struct zmq_tx_input_conf* zmq_input_conf = (struct zmq_tx_input_conf*)zmq_input->init_conf();
	zmq_input_conf->address = calloc(ZMQ_URI_LEN, sizeof(char));
	zmq_input_conf->address_tick = calloc(ZMQ_URI_LEN, sizeof(char));
	snprintf(zmq_input_conf->address, ZMQ_URI_LEN, "tcp://127.0.0.1:%d", modem_base + 1);
	snprintf(zmq_input_conf->address_tick , ZMQ_URI_LEN, "tcp://127.0.0.1:%d", modem_base + 3);
	zmq_input_conf->binding = 1;
	zmq_input_conf->binding_ticks = 1;
	zmq_input_conf->thread = 1;


	void *zmq_input_inst = zmq_input->init(zmq_input_conf);
	assert(zmq_input_inst);


	framer->set_frame_source(framer_inst, zmq_input->source_frame, zmq_input_inst);
	transmitter->set_symbol_source(transmitter_inst, framer->source_symbols, framer_inst);
	sdr->set_sample_source(sdr_inst, transmitter->source_samples, transmitter_inst);

	//zmq_input->set_frame_sink(zmq_input_inst, transmitter, transmitter_inst);
	//framer->set_frame_source(framer_inst, zmq_input, zmq_input_inst);

	//deframer->set_frame_sink(deframer_9k6_inst, zmq_input, zmq_input_inst);
	//deframer->set_frame_sink(deframer_12k6_inst, zmq_input, zmq_input_inst);



	zmq_input->reset(zmq_input_inst);

	/*
	 * Run!
	 */
	int ret = sdr->execute(sdr_inst);
	fprintf(stderr, "Suo exited %d\n", ret);

	if (ret < 0)
		return 1;
	return 0;
}
