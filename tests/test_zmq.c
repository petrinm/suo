
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <CUnit/CUnit.h>
#include <CUnit/CUnitCI.h>

#include "suo.h"
#include "frame-io/zmq_interface.h"


#define SLEEP(x) do { usleep((x) * 1000); time += (x) * 1000; } while(0)


void test_zmq_simple(void) {

	timestamp_t time = 0;

	/*
	 * Create ZMQ TX input
	 */
	const struct tx_input_code *in_code = &zmq_tx_input_code;
	CU_ASSERT(in_code->name != NULL);

	void* in_conf = in_code->init_conf();
	CU_ASSERT(in_code->set_conf(in_conf, "address", "tcp://127.0.0.1:43301") == 0);
	CU_ASSERT(in_code->set_conf(in_conf, "address_tick", "tcp://127.0.0.1:43303") == 0);
	CU_ASSERT(in_code->set_conf(in_conf, "flags", "6") == 0); // ZMQIO_BIND | ZMQIO_METADATA | ZMQIO_THREAD | ZMQIO_BIND_TICK

	void* in_inst = in_code->init(in_conf);
	CU_ASSERT_FATAL(in_inst != NULL);

	SLEEP(1000);

	/*
	 * Create ZMQ RX output
	 */
	const struct rx_output_code *out_code = &zmq_rx_output_code;
 	CU_ASSERT(out_code->name != NULL);

	void* out_conf = out_code->init_conf();
	CU_ASSERT(out_code->set_conf(out_conf, "address", "tcp://127.0.0.1:43301") == 0);
	CU_ASSERT(out_code->set_conf(out_conf, "address_tick", "tcp://127.0.0.1:43303") == 0);
	CU_ASSERT(out_code->set_conf(out_conf, "flags", "15") == 0); // 6 = ZMQIO_METADATA | ZMQIO_THREAD

	void* out_inst = out_code->init(out_conf);
	CU_ASSERT_FATAL(out_inst != NULL);

	SLEEP(200);

	/*
	 * Create test frame
	 */
	struct frame* out_frame = suo_frame_new(256);
	out_frame->hdr.timestamp = time;
	SET_METADATA_F(out_frame, METADATA_CFO, 12.345);
	SET_METADATA_F(out_frame, METADATA_RSSI, -123.4);
	for (int i = 0; i < 64; i++)
		out_frame->data[i] = rand() % 256;
	out_frame->data_len = 64;


	printf("Sent:\n");
	suo_frame_print(out_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

	SLEEP(600);

	/*
	 * Send the frame twice
	 */
	CU_ASSERT_FATAL(out_code->sink_frame(out_inst, out_frame, time) == 0);
	CU_ASSERT_FATAL(out_code->sink_frame(out_inst, out_frame, time) == 0);

	SLEEP(200);

	/*
	 * Receive the frame
	 */
	struct frame* in_frame = suo_frame_new(256);
	CU_ASSERT_FATAL(in_code->source_frame(in_inst, in_frame, time) == 1);

	printf("Received:\n");
	suo_frame_print(in_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA);

	CU_ASSERT(in_frame->data_len == out_frame->data_len);
	CU_ASSERT(memcmp(in_frame->data, out_frame->data, in_frame->data_len) == 0);
	suo_frame_clear(in_frame);

	// Receive second
	CU_ASSERT_FATAL(in_code->source_frame(in_inst, in_frame, time) == 1);

	printf("Received:\n");
	suo_frame_print(in_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA);
	CU_ASSERT(in_frame->data_len == out_frame->data_len);
	CU_ASSERT(memcmp(in_frame->data, out_frame->data, in_frame->data_len) == 0);



	// Third time
	CU_ASSERT_FATAL(in_code->source_frame(in_inst, in_frame, time) == 0);

	suo_frame_destroy(out_frame);
	suo_frame_destroy(in_frame);

	/*
	 * Test ticks
	 */
	{

	}
	/*
	 * Clean out
	 */
	out_code->destroy(out_inst);
	in_code->destroy(in_inst);

}

CUNIT_CI_RUN("test_zmq",
	CUNIT_CI_TEST(test_zmq_simple)
);
