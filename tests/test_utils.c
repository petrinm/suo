#include <assert.h>
#include <stdio.h>

#include <CUnit/CUnit.h>
#include <CUnit/CUnitCI.h>

#include "suo.h"
#include "framing/golay_framer.h"
#include "framing/golay_deframer.h"
#include "assert_metadata.h"



static void test_frame_operations(void)
{

	srand((int)time(NULL));


	/* Create a test frame */
	struct frame * frame_a = suo_frame_new(256);
	frame_a->hdr.id = rand() & 0xffff;
	frame_a->hdr.timestamp = rand();
	frame_a->data_len = rand() % 256;
	for (unsigned i = 0; i < frame_a->data_len; i++)
		frame_a->data[i] = rand() % 256;

	SET_METADATA_I(frame_a, METADATA_SYNC_ERRORS, -1);
	SET_METADATA_UI(frame_a, METADATA_GOLAY_CODED, 1337);
	SET_METADATA_F(frame_a, METADATA_CFO, 12.345);
	SET_METADATA_D(frame_a, METADATA_RSSI, -123.4);
	CU_ASSERT(suo_metadata_count(frame_a) == 4);
	printf("META COUNT %d\n", suo_metadata_count(frame_a));
	suo_frame_print(frame_a, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);


	/* Clone the frame A */
	struct frame *frame_b = suo_frame_new(256);
	suo_frame_copy(frame_b, frame_a);
	suo_frame_print(frame_b, SUO_PRINT_DATA | SUO_PRINT_METADATA);

	CU_ASSERT(frame_b->hdr.timestamp == frame_a->hdr.timestamp);
	CU_ASSERT(frame_b->data_len == frame_a->data_len);
	CU_ASSERT(memcmp(frame_b->data, frame_a->data, frame_b->data_len) == 0);
	CU_ASSERT(memcmp(frame_b->metadata, frame_a->metadata, MAX_METADATA * sizeof(struct metadata)) == 0);

	/* Assert metadata */
	ASSERT_METADATA_INT(frame_b, METADATA_SYNC_ERRORS, -1);
	ASSERT_METADATA_EXISTS(frame_b, METADATA_SYNC_ERRORS, METATYPE_INT);
	ASSERT_METADATA_UINT(frame_b, METADATA_GOLAY_CODED, 1337);
	ASSERT_METADATA_FLOAT(frame_b, METADATA_CFO, 12.345);
	ASSERT_METADATA_DOUBLE(frame_b, METADATA_RSSI, -123.4);


	/* Second clonging */
	struct frame *frame_c = calloc(1, sizeof(struct frame));
	suo_frame_copy(frame_c, frame_a);
	suo_frame_print(frame_c, SUO_PRINT_DATA | SUO_PRINT_METADATA);

	CU_ASSERT(frame_b->hdr.timestamp == frame_a->hdr.timestamp);
	CU_ASSERT(frame_b->data_len == frame_a->data_len);
	CU_ASSERT(memcmp(frame_b->data, frame_a->data, frame_b->data_len) == 0);
	CU_ASSERT(memcmp(frame_b->metadata, frame_a->metadata, MAX_METADATA * sizeof(struct metadata)) == 0);




	/* Cleaning */
	suo_frame_destroy(frame_a); frame_a = NULL;
	suo_frame_destroy(frame_b); frame_b = NULL;
	suo_frame_destroy(frame_c); frame_c = NULL;
	suo_frame_destroy(NULL);



#if 0

	suo_frame_print(received_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA);

	CU_ASSERT(transmit_frame->data_len == received_frame->data_len);
	CU_ASSERT(memcpy(transmit_frame->data, received_frame->data, received_frame->data_len) == 0);


	ASSERT_METADATA_EXISTS(received_frame, METADATA_GOLAY_CODED, METATYPE_UINT);
	ASSERT_METADATA_EXISTS(received_frame, METADATA_GOLAY_ERRORS, METATYPE_UINT);


#endif

}

static void test_bit_operations(void) {

	/*
	size_t bytes_to_bits(bit_t *bits, const uint8_t *bytes, size_t nbytes, bool lsb_first);
	size_t word_to_lsb_bits(bit_t *bits, size_t nbits, uint64_t word);
	size_t word_to_msb_bits(bit_t *bits, size_t nbits, uint64_t word);
	*/
}

CUNIT_CI_RUN("test_utils",
	CUNIT_CI_TEST(test_frame_operations),
	CUNIT_CI_TEST(test_bit_operations)
);
