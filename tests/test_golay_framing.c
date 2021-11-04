#include <assert.h>
#include <stdio.h>

#include <CUnit/CUnit.h>
#include <CUnit/CUnitCI.h>

#include "suo.h"
#include "framing/golay_framer.h"
#include "framing/golay_deframer.h"

#include "assert_metadata.h"

static const struct frame* received_frame = NULL;

static int dummy_frame_sink(void *arg, const struct frame *frame) {
	(void)arg;
	received_frame = suo_frame_new(frame->data_len);
	suo_frame_copy(received_frame, frame);
	return SUO_OK;
}

static int dummy_frame_source(void *arg, struct frame *frame) {
	suo_frame_copy(frame, arg);
	return 1;
}


#define RANDOM_BIT()  ((unsigned int)(rand() & 1))

static void test_golay_simple(void)
{

	struct frame *transmit_frame = NULL;
	symbol_t* symbols = calloc(1, 1024);

	{
		timestamp_t t = 0;
		const struct encoder_code* framer = &golay_framer_code;
		CU_ASSERT(framer->name != NULL);

		void* conf = framer->init_conf();
		CU_ASSERT(conf != NULL);
		CU_ASSERT(framer->set_conf(conf, "sync_word", "3735928559") == 0);
		CU_ASSERT(framer->set_conf(conf, "sync_len", "32") == 0);
		CU_ASSERT(framer->set_conf(conf, "preamble_len", "64") == 0);
		CU_ASSERT(framer->set_conf(conf, "use_viterbi", "0") == 0);
		CU_ASSERT(framer->set_conf(conf, "use_randomizer", "1") == 0);
		CU_ASSERT(framer->set_conf(conf, "use_rs", "0") == 0);
		CU_ASSERT(framer->set_conf(conf, "tail_length", "0") == 0);

		void* inst = framer->init(conf);
		CU_ASSERT(inst != NULL);


		/* Create a test frame */
		transmit_frame = suo_frame_new(28);
		transmit_frame->hdr.id = 2;
		transmit_frame->hdr.timestamp = 1234;
		transmit_frame->data_len = 28;
		for (unsigned i = 0; i < 28; i++)
			transmit_frame->data[i] = i;

		framer->set_frame_source(inst, dummy_frame_source, transmit_frame);

		suo_frame_print(transmit_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

		/* Encode frame to bits */
		int ret = framer->get_symbols(inst, symbols, 1024, t);
		printf("Output symbols = %d\n", ret);
		CU_ASSERT_FATAL(ret == 64 + 32 + 24 + 8 * 28);

		printf("Preamble:\n");
		for (int i = 0; i < 64; i++) printf("%d ", symbols[i]);
		printf("\n\n");

		printf("Sync:\n");
		for (int i = 64; i < 64+32; i++) printf("%d ", symbols[i]);
		printf("\n\n");

		printf("Golay:\n");
		for (int i = 64+32; i < 64+32+24; i++) printf("%d ", symbols[i]);
		printf("\n\n");

		printf("Data:\n");
		for (int i = 64+32+24; i < ret; i++) printf("%d ", symbols[i]);
		printf("\n\n");

		/* Clean up */
		framer->destroy(inst);
		CU_PASS("Framing done");
	}

	/*
	 * Decoding test
	 */
	{

		symbols[64 + 2] ^= 1;  // Syncword error
		symbols[64 + 32 + 9] ^= 1;  // Golay bit error

		const struct decoder_code* deframer = &golay_deframer_code;
		CU_ASSERT(deframer->name != NULL);

		void* conf = deframer->init_conf();
		CU_ASSERT(conf != NULL);
		CU_ASSERT(deframer->set_conf(conf, "sync_word", "3735928559") == 0);
		CU_ASSERT(deframer->set_conf(conf, "sync_len", "32") == 0);
		CU_ASSERT(deframer->set_conf(conf, "sync_threshold", "3") == 0);
		CU_ASSERT(deframer->set_conf(conf, "skip_viterbi", "1") == 0);
		CU_ASSERT(deframer->set_conf(conf, "skip_randomizer", "0") == 0);
		CU_ASSERT(deframer->set_conf(conf, "skip_rs", "1") == 0);

		void* inst = deframer->init(conf);
		CU_ASSERT(inst != NULL);

		deframer->set_frame_sink(inst, dummy_frame_sink, 0xdeadbeef);


		timestamp_t t = 10000;
		int ret;

		received_frame = NULL;

		struct frame *frame = suo_frame_new(256);

		/*
		 * Feed some random symbols + bit from previous test
		 */
		for (int i = 0; i < 100; i++)
			ret = deframer->execute(inst, RANDOM_BIT(), t++);
		for (int i = 0; i < 344; i++)
			ret = deframer->execute(inst, symbols[i], t++);
		for (int i = 0; i < 100; i++)
			ret = deframer->execute(inst, RANDOM_BIT(), t++);

		CU_ASSERT_FATAL(received_frame != NULL);
		suo_frame_print(received_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA);

		CU_ASSERT(received_frame->hdr.timestamp != 0);
		CU_ASSERT(received_frame->data_len == transmit_frame->data_len);
		CU_ASSERT(memcmp(transmit_frame->data, received_frame->data, received_frame->data_len) == 0);


		ASSERT_METADATA_UINT(received_frame, METADATA_SYNC_ERRORS, 1);
		ASSERT_METADATA_EXISTS(received_frame, METADATA_GOLAY_CODED, METATYPE_UINT);
		ASSERT_METADATA_UINT(received_frame, METADATA_GOLAY_ERRORS, 1);

		// Check stats
		//CU_ASSERT(dunno->frames_detected == 1);


		/* Clean up */
		deframer->destroy(inst);
		//free(frame);
		CU_PASS("Receiving OK");
	}

	suo_frame_destroy(transmit_frame); transmit_frame = NULL;
	suo_frame_destroy(received_frame); received_frame = NULL;

	free(symbols);
}


CUNIT_CI_RUN("test_golay",
	CUNIT_CI_TEST(test_golay_simple)
);
