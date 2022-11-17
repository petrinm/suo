#include "signal-io/file_io.hpp"
#include "signal-io/conversion.hpp"
#include "registry.hpp"

#include <stdio.h>
#include <assert.h>


using namespace std;
using namespace suo;

enum inputformat { 
	FORMAT_CU8,
	FORMAT_CS16,
	FORMAT_CF32
};

// TODO configuration for BUFLEN
#define BUFLEN 4096


FileIO::Config::Config() {
	format = "CF32";
	throttle = true;
}

FileIO::FileIO(const Config& conf) :
	conf(conf)
{
	if (conf.input.empty() != false)
		in = fopen(conf.input.c_str(), "rb");
	else
		in = stdin;
	if (in == NULL)
		perror("Failed to open signal input");

	if (conf.output.empty() != false)
		out = fopen(conf.output.c_str(), "wb");
	else
		out = stdout;

}


FileIO::~FileIO()
{
	if (in)
		fclose(in);
	if (out)
		fclose(out);
}

#if 0

void FileIO::execute()
{
	enum inputformat inputformat = conf.format;
	Timestamp timestamp = 0;
	Timestamp tx_latency_time = 0;
	sample_t buf2[BUFLEN];


	for(;;) {
		size_t n = BUFLEN;

		// RX
		if (sink_samples != NULL) {
			if(inputformat == FORMAT_CU8) {
				cu8_t buf1[BUFLEN];
				n = fread(buf1, sizeof(cu8_t), BUFLEN, in);
				if(n == 0) break;
				cu8_to_cf(buf1, buf2, n);
			} else if(inputformat == FORMAT_CS16) {
				cs16_t buf1[BUFLEN];
				n = fread(buf1, sizeof(cs16_t), BUFLEN, in);
				if(n == 0) break;
				cs16_to_cf(buf1, buf2, n);
			} else {
				n = fread(buf2, sizeof(sample_t), BUFLEN, in);
				if(n == 0) break;
			}
			sinkSamples(buf2, n, timestamp);
		}

		// TX
		if (source_samples != NULL) {
			assert(n <= BUFLEN);
			int tr = sourceSamples(buf2, n, timestamp + tx_latency_time);
			fwrite(buf2, sizeof(sample_t), tr, out);
		}

		timestamp += 1e9 * n / conf.samplerate;

		if (conf.throttle) {
			nsleep(1e9 * n / conf.samplerate);
		}
	}

}
#endif



Block* createFileIO(const Kwargs& args)
{
	return new FileIO();
}

static Registry registerFileIO("FileIO", &createFileIO);
