#include <assert.h>
#include <stdio.h>

#include <CUnit/CUnit.h>
#include <CUnit/CUnitCI.h>

#include <liquid/liquid.h>

#include "suo.h"
#include "modem/mod_gmsk.h"
#include "modem/demod_gmsk.h"

static const struct frame* received_frame = NULL;


static int source_dummy_symbols(void *arg, symbol_t *symbols, size_t max_symbols, timestamp_t timestamp)
{
	static int f = 0;
	if (f == 1)
		return 0;
	 f = 1;

	(void)arg; (void)timestamp;
	for (int i = 0; i < 32; i++)
		*symbols++ = (i & 1);

	*symbols++ = 0;
	*symbols++ = 1;
	*symbols++ = 1;
	*symbols++ = 1;

	*symbols++ = 1;
	*symbols++ = 0;
	*symbols++ = 1;
	*symbols++ = 0;

	return 32 + 8;
}


static int source_dummy_frame(void * v, struct frame *frame, timestamp_t t) {
	(void)v; (void)t;

	frame->hdr.id = 2;
	frame->data_len = 28;
	for (unsigned i = 0; i < 28; i++)
		frame->data[i] = i;

	return SUO_OK;
}





static void test_gmsk(void)
{
#if 0
	bit_t* bits = calloc(1, 1024);

	// frame detector
	unsigned preamble_len = 63;
	msequence ms = msequence_create(6, 0x6d, 1);

	printf("\n\n");
	for (int i=0; i<preamble_len + 6; i++) {
		unsigned char bit = msequence_advance(ms);
		printf("%d ", bit);
	}
	printf("\n\n");
#endif


#if 0
	unsigned int k = 4; // filter samples/symbol
	unsigned int m = 3; // filter semi-length (symbols)
	float bt = 0.3; // filter bandwidth-time product
	unsigned int npfb = 32; // number of filters in symsync

	firpfb_rrrf mf = firpfb_rrrf_create_rnyquist(LIQUID_FIRFILT_GMSKRX, npfb, k, m, bt);
	firpfb_rrrf dmf = firpfb_rrrf_create_drnyquist(LIQUID_FIRFILT_GMSKRX, npfb, k, m, bt);

	firpfb_rrrf_print(mf);
	firpfb_rrrf_print(dmf);

#endif


#if 1
#define MAX_SAMPLES 500

	sample_t samples[MAX_SAMPLES];
	{

		timestamp_t time = 10000;

		const struct transmitter_code* gmsk = &mod_gmsk_code;
		CU_ASSERT(gmsk->name != NULL);

		struct mod_gmsk_conf* gmsk_conf = (struct mod_gmsk_conf*)gmsk->init_conf();
		gmsk_conf->samplerate = 50e3;
		gmsk_conf->symbolrate = 9600;
		gmsk_conf->centerfreq = 10e3;
		gmsk_conf->bt = 0.3;


		void* gmsk_inst = gmsk->init(gmsk_conf);
		gmsk->set_symbol_source(gmsk_inst, source_dummy_symbols, (void*)0xdeadbeef);

		int ret;
		unsigned int total_samples = 0; 
		for (int i = 0; i < 1000; i++) {

			ret = gmsk->source_samples(gmsk_inst, samples, MAX_SAMPLES, time);
			printf("ret = %d\n", ret);
			CU_ASSERT_FATAL(ret >= 0);
			if (ret == 0)
				break;

			total_samples += ret;
			time += 1000;
		}
		printf("total samples = %u\n", total_samples);
		CU_ASSERT(total_samples > 0);

		// TODO: Change frequency
		// gmsk->set_frequency(gmsk_inst, 11e3);
	}

#endif
}


CUNIT_CI_RUN("test_gmsk",
	CUNIT_CI_TEST(test_gmsk)
);
