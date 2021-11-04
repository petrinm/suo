#include <assert.h>
#include <stdio.h>

#include <CUnit/CUnit.h>
#include <CUnit/CUnitCI.h>

#include <liquid/liquid.h>

#include "suo.h"
#include "modem/mod_gmsk.h"
#include "modem/demod_gmsk.h"

static const struct frame* received_frame = NULL;

#define RANDOM_BIT()  ((unsigned int)(rand() & 1)

#if 0

const struct transmitter_code dummy_sample_source = {
	.name = "dummy_sample_sink",
	.init = dummy_sample_sink_init,
	.sink_sample = dummy_sink_samples,
};

const struct receiver_code dummy_sample_sink = {
	.name = "dummy_sample_sink",
	.init = dummy_sample_sink_init,
	.sink_sample = dummy_sink_samples,
};


const struct decoder_code dummy_symbol_sink = {
	.name = "dummy_symbol_sink",
	.init = dummy_symbol_sink_init,
	.sink_symbols = dummy_sink_symbols,
};

const struct encoder_code dummy_symbol_source = {
	.name = "dummy_symbol_source",
	.init = dummy_symbol_source_init,
	.source_symbols = dummy_source_symbols,
};

const struct rx_input_code dummy_frame_sink = {
	.name = "dummy_frame_sink",
	.init = dummy_frame_sink_init,
	.sink_frame = dummy_sink_frames,
};
#endif


#if 0
struct dummy_frames_source {
	const struct decoder_code *decoder;
	void *decoder_arg;
};

void* dummy_frame_source_init(const void *conf) {
	(void)conf;
	struct dummy_frames_source *self = (struct dummy_frames_source*)calloc(1, sizeof(struct dummy_frames_source));

	return self;
}

static int dummy_source_frame(void* arg, struct frame *frame, size_t maxlen, timestamp_t timenow) {
	return SUO_OK;
}

const struct tx_input_code dummy_frame_source = {
	.name = "dummy_frame_source",
	.init = dummy_frame_source_init,
	.source_frame = dummy_source_frame,
};
#endif


static int source_dummy_symbols(void *arg, symbol_t *symbols, size_t max_symbols, timestamp_t timestamp)
{
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
		const struct transmitter_code* gmsk = &mod_gmsk_code;
		CU_ASSERT(gmsk->name != NULL);

		struct mod_gmsk_conf* gmsk_conf = (struct mod_gmsk_conf*)gmsk->init_conf();
		gmsk_conf->samplerate = 50e3;
		gmsk_conf->symbolrate = 9600;
		gmsk_conf->centerfreq = 10e3;
		gmsk_conf->bt = 0.3;


		void* gmsk_inst = gmsk->init(gmsk_conf);
		gmsk->set_symbol_source(gmsk_inst, source_dummy_symbols, 0xdeadbeef);


		timestamp_t t = 10000;
		CU_ASSERT(gmsk->source_samples(gmsk_inst, samples, MAX_SAMPLES, t++) > 0);
		CU_ASSERT(gmsk->source_samples(gmsk_inst, samples, MAX_SAMPLES, t++) > 0);
		CU_ASSERT(gmsk->source_samples(gmsk_inst, samples, MAX_SAMPLES, t++) > 0);
		CU_ASSERT(gmsk->source_samples(gmsk_inst, samples, MAX_SAMPLES, t++) > 0);


		// TODO: Change frequency
		// gmsk->set_frequency(gmsk_inst, 11e3);
	}

#endif
}


CUNIT_CI_RUN("test_gmsk",
	CUNIT_CI_TEST(test_gmsk)
);
