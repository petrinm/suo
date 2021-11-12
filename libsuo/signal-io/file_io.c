#include "file_io.h"
#include "suo_macros.h"
#include "conversion.h"
#include <stdio.h>
#include <assert.h>


enum inputformat { FORMAT_CU8, FORMAT_CS16, FORMAT_CF32 };

struct file_io {
	struct file_io_conf conf;

	FILE *in, *out;

	CALLBACK(sample_sink_t, sink_samples);
	CALLBACK(sample_source_t, source_samples);
};


static void *file_io_init(const void *conf)
{
	struct file_io *self;
	self = calloc(1, sizeof(*self));
	if (self == NULL)
		return self;
	self->conf = *(struct file_io_conf*)conf;

	if (self->conf.input != NULL)
		self->in = fopen(self->conf.input, "rb");
	else
		self->in = stdin;
	if (self->in == NULL)
		perror("Failed to open signal input");

	if (self->conf.output != NULL)
		self->out = fopen(self->conf.output, "wb");
	else
		self->out = stdout;

	return self;
}


static int file_io_destroy(void *arg)
{
	struct file_io *self = arg;
	if (self->in)
		fclose(self->in);
	if (self->out)
		fclose(self->out);
	return 0;
}


static int file_io_set_sample_sink(void *arg, sample_sink_t callback, void *callback_arg) {
	struct file_io *self = arg;
	self->sink_samples = callback;
	self->sink_samples_arg = callback_arg;
	return SUO_OK;
}


static int file_io_set_sample_source(void *arg, sample_source_t callback, void *callback_arg) {
	struct file_io *self = arg;
	self->source_samples = callback;
	self->source_samples_arg = callback_arg;
	return SUO_OK;
}



// TODO configuration for BUFLEN
#define BUFLEN 4096

static int file_io_execute(void *arg)
{
	struct file_io *self = arg;
	enum inputformat inputformat = self->conf.format;
	suo_timestamp_t timestamp = 0, tx_latency_time = 0;
	sample_t buf2[BUFLEN];

	if (self->in == NULL)
		return -1;
	for(;;) {
		size_t n = BUFLEN;

		// RX
		if (self->sink_samples != NULL) {
			if(inputformat == FORMAT_CU8) {
				cu8_t buf1[BUFLEN];
				n = fread(buf1, sizeof(cu8_t), BUFLEN, self->in);
				if(n == 0) break;
				cu8_to_cf(buf1, buf2, n);
			} else if(inputformat == FORMAT_CS16) {
				cs16_t buf1[BUFLEN];
				n = fread(buf1, sizeof(cs16_t), BUFLEN, self->in);
				if(n == 0) break;
				cs16_to_cf(buf1, buf2, n);
			} else {
				n = fread(buf2, sizeof(sample_t), BUFLEN, self->in);
				if(n == 0) break;
			}
			self->sink_samples(self->sink_samples_arg, buf2, n, timestamp);
		}

		// TX
		if (self->source_samples != NULL) {
			assert(n <= BUFLEN);
			int tr = self->source_samples(self->source_samples_arg, buf2, n, timestamp + tx_latency_time);
			fwrite(buf2, sizeof(sample_t), tr, self->out);
		}

		timestamp += 1e9 * n / self->conf.samplerate;
	}

	return 0;
}


const struct file_io_conf file_io_defaults = {
	.samplerate = 1e6,
	.input = NULL,
	.output = NULL,
	.format = 1
};

CONFIG_BEGIN(file_io)
CONFIG_F(samplerate)
CONFIG_C(input)
CONFIG_C(output)
CONFIG_I(format)
CONFIG_END()


const struct signal_io_code file_io_code = {
	.name = "file_io",
	.init = file_io_init,
	.destroy = file_io_destroy,
	.init_conf = init_conf,  // Constructed by CONFIG-macro
	.set_conf =  set_conf, // Constructed by CONFIG-macro
	.set_sample_sink = file_io_set_sample_sink,
	.set_sample_source = file_io_set_sample_source,
	.execute = file_io_execute
};
