#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <CUnit/CUnit.h>
#include <CUnit/CUnitCI.h>

#include <liquid/liquid.h>

#include "suo.h"
#include "modem/mod_gmsk.h"
#include "modem/demod_gmsk.h"
#include "modem/demod_fsk_mfilt.h"
#include "framing/golay_framer.h"
#include "framing/golay_deframer.h"


static struct frame* transmit_frame = NULL;
static struct frame* received_frame = NULL;

static int dummy_frame_sink(void *arg, const struct frame *frame, suo_timestamp_t t) {
	(void)arg; (void)t;
	received_frame = suo_frame_new(frame->data_len);
	suo_frame_copy(received_frame, frame);
	suo_frame_print(frame, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);
	return SUO_OK;
}


static int source_dummy_frame(void * arg, struct frame *frame, suo_timestamp_t time) {
	(void)arg;

#if 0
	static last = 0;
	if ((last - time) < 100)
		return 0;
	last = time;
#endif

	// Generate a new frame
	static unsigned int id = 1;
	frame->hdr.id = id++;
	frame->data_len = 200;
	//frame->data_len = 32 + rand() % 128;
	for (unsigned i = 0; i < frame->data_len; i++)
		frame->data[i] = rand() % 256;

	suo_frame_print(frame, SUO_PRINT_DATA);

	// Copy the frame for later asserting
	if (transmit_frame == NULL)
		transmit_frame = suo_frame_new(256);
	suo_frame_copy(transmit_frame, frame);
	return 1;
}


void delay_signal(float delay, sample_t* samples, unsigned int n_samples)
{
	unsigned int h_len       = 19;      // filter length
	unsigned int p           = 5;       // polynomial order
	float        fc          = 0.45f;   // filter cutoff
	float        As          = 60.0f;   // stop-band attenuation [dB]

	firfarrow_crcf f = firfarrow_crcf_create(h_len, p, fc, As);
	firfarrow_crcf_set_delay(f, delay);

	for (unsigned int i = 0; i < n_samples; i++) {
		firfarrow_crcf_push(f, samples[i]);
		firfarrow_crcf_execute(f, &samples[i]);
	}

	firfarrow_crcf_destroy(f);
}


static void test_txrx(void)
{
#define MAX_SAMPLES 50000
	srand(time(NULL));

	sample_t samples[MAX_SAMPLES];
	{

		/* Contruct framer */
		const struct encoder_code* framer = &golay_framer_code;
		CU_ASSERT_FATAL(framer->name != NULL);
		struct golay_framer_conf* framer_conf = framer->init_conf();
		CU_ASSERT_FATAL(framer_conf != NULL);
		framer_conf->sync_word = 0xC9D08A7B;
		framer_conf->sync_len = 32;
		framer_conf->preamble_len = 64;
		framer_conf->use_viterbi = 0;
		framer_conf->use_randomizer = 0;
		framer_conf->use_rs = 0;
		framer_conf->tail_length = 60;

		void* framer_inst = framer->init(framer_conf);
		CU_ASSERT_FATAL(framer_inst != NULL);
		framer->set_frame_source(framer_inst, source_dummy_frame, (void*)0xdeadbeef);


		/* Contruct modulator */
		const struct transmitter_code* mod = &mod_gmsk_code;
		CU_ASSERT_FATAL(mod->name != NULL);
		struct mod_gmsk_conf* mod_conf = (struct mod_gmsk_conf*)mod->init_conf();
		CU_ASSERT_FATAL(mod_conf != NULL);
		mod_conf->samplerate = 50e3;
		mod_conf->symbolrate = 9600;
		mod_conf->centerfreq = 10e3;
		mod_conf->bt = 0.5;

		void* mod_inst = mod->init(mod_conf);
		mod->set_symbol_source(mod_inst, framer->source_symbols, framer_inst);



		/* Construct deframer */
		const struct decoder_code* deframer = &golay_deframer_code;
		CU_ASSERT_FATAL(deframer->name != NULL);

		struct golay_deframer_conf* deframer_conf = deframer->init_conf();
		CU_ASSERT_FATAL(deframer_conf != NULL);
		deframer_conf->sync_word = 0xC9D08A7B;
		deframer_conf->sync_len = 32;
		deframer_conf->sync_threshold = 3;
		deframer_conf->skip_viterbi = 1;
		deframer_conf->skip_randomizer = 0;
		deframer_conf->skip_rs = 1;

		void* deframer_inst = deframer->init(deframer_conf);
		CU_ASSERT_FATAL(deframer_inst != NULL);
		deframer->set_frame_sink(deframer_inst, dummy_frame_sink, (void*)0xdeadbeef);

#if 1
		/* Construct demodulator */
		const struct receiver_code* demod = &demod_fsk_mfilt_code;
		CU_ASSERT_FATAL(demod->name != NULL);
		struct fsk_demod_mfilt_conf* demod_conf = (struct fsk_demod_mfilt_conf*)demod->init_conf();
		CU_ASSERT_FATAL(demod_conf != NULL);
		demod_conf->sample_rate = 50e3;
		demod_conf->symbol_rate = 9600;
		demod_conf->center_frequency = 10e3;

		void* demod_inst = demod->init(demod_conf);
		demod->set_symbol_sink(demod_inst, deframer->sink_symbol, deframer_inst);
#else
		/* Construct demodulator */
		const struct receiver_code* demod = &demod_gmsk_code;
		CU_ASSERT_FATAL(demod->name != NULL);
		struct demod_gmsk_conf* demod_conf = (struct demod_gmsk_conf*)demod->init_conf();
		CU_ASSERT_FATAL(demod_conf != NULL);
		demod_conf->sample_rate = 50e3;
		demod_conf->symbol_rate = 9600;
		demod_conf->center_frequency = 11e3;
		demod_conf->sync_word = 0xC9D08A7B;
		demod_conf->sync_len = 32;

		void* demod_inst = demod->init(demod_conf);
		demod->set_symbol_sink(demod_inst, deframer->sink_symbol, deframer_inst);
#endif


		int ret;
		suo_timestamp_t time = 10000;

		float SNRdB = 3; // -5 to 10
		float nstd = powf(10.0f, -SNRdB/20.0f); // noise standard deviation


		unsigned int noise_samples = rand() % MAX_SAMPLES;
		for (unsigned i=0; i < noise_samples; i++)
			samples[i] = nstd*(randnf() + _Complex_I*randnf()) * M_SQRT1_2;
		ret = demod->sink_samples(demod_inst, samples, noise_samples, time);


		// Modulate the signal
		ret = mod->source_samples(mod_inst, samples, MAX_SAMPLES, time);
		printf("ret = %d\n", ret);
		CU_ASSERT_FATAL(ret > 0);

#if 0
		// Add imparities (noise)
		for (unsigned i=0; i < ret; i++)
			samples[i] += nstd*(randnf() + _Complex_I*randnf()) * M_SQRT1_2;
#endif
		delay_signal((float)(rand() % 1000) / 1000, samples, ret);

		// Demodulate the signal
		ret = demod->sink_samples(demod_inst, samples, ret, time);
		printf("ret = %d\n", ret);
		CU_ASSERT_FATAL(ret == 0);

		// Assert the transmit and received frames
		CU_ASSERT_FATAL(received_frame != NULL);
		CU_ASSERT(received_frame->data_len == transmit_frame->data_len);
		CU_ASSERT(memcmp(transmit_frame->data, received_frame->data, received_frame->data_len) == 0);


		unsigned int total_errors = 0;
		unsigned int total_bits = 0;

		for (unsigned int i = 0; i < transmit_frame->data_len; i++) {
			total_errors += __builtin_popcountll(transmit_frame->data[i] ^ received_frame->data[i]);
			total_bits += 8;
		}

		printf("Total error: %u\n", total_errors);
		printf("Total bits: %u\n", total_bits);
		printf("BER = %.1e\n", (double)total_errors / (double)total_bits);


	}


}


CUNIT_CI_RUN("test_txrx",
	CUNIT_CI_TEST(test_txrx)
);
