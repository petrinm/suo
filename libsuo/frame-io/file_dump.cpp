#include <iostream>
#include <fstream>
#include <iomanip>

#include "file_dump.hpp"
#include "registry.hpp"


using namespace suo;
using namespace std;

FileDump::Config::Config() :
	format(FormatASCII)
{}

FileDump::FileDump(const Config& _conf) :
	conf(_conf)
{
	if (conf.filename.empty() == false)
		open(conf.filename);
}


void FileDump::open(const std::string& filename) {

	if (output.is_open())
		output.close();

	ios_base::openmode flags = ios::out | ios::app;
	if (conf.format == FormatRaw || conf.format == FormatKISS)
		flags |= ios::binary;

	output.open(filename, flags);
	if (output.is_open() == false)
		throw SuoError("FileDump: Failed to open output file %s", filename.c_str());

}


void FileDump::close() {
	output.close();
}




void FileDump::sinkFrame(const Frame& frame, Timestamp timestamp) {

	if (output.is_open() == false)
		throw SuoError("FileDump: No output stream opened!");

	switch (conf.format) {
	case FormatRaw: {
		/* Output as raw binary to file */
		output.write(reinterpret_cast<const char*>(frame.raw_ptr()), frame.size());
		break;
	}
	case FormatKISS: {
		/* Output as raw binary using KISS framing */
		const uint8_t FEND = 0xC0; // Frame End
		const uint8_t FESC = 0xDB; // Frame Escape
		const uint8_t TFEND = 0xDC; // Transposed Frame End
		const uint8_t TFESC = 0xDD; // Transposed Frame Escape

		output << FEND << 0;
		for (Byte b: frame.data) {
			if (b == FEND)
				output << FESC << TFEND;
			else if (b == FESC)
				output << FESC << TFESC;
			else
				output << b;
		}
		output << FEND;

		break;
	}
	case FormatASCII: {
		/* Print frame using Suo's verbose ASCII formatting. */
		output << frame(Frame::PrintData | Frame::PrintMetadata);
		break;
	}
	case FormatASCIIHex: {
		/* Print frame data as hexadecimal string to file. */
		output << hex << right << setfill('0');
		for (unsigned int i = 0; i < frame.data.size(); i++) {
			output << setw(2) << (int)frame.data[i];
		}
		output << endl;
		output.flush();
		break;
	}
	default:
		throw SuoError("FileDump: Invalid output format %d", conf.format);
	}
}


#if 0

void* file_frame_output_init(const void* conf_v) {
	struct file_frame_dump* self = calloc(1, sizeof(struct file_frame_dump));
	self->c = *(struct file_frame_output_conf*)conf_v;

	if (strcmp(self->c.filename, "-") == 0)
		self->stream = stdout;
	else if (strcmp(self->c.filename, "-") == 0)
		self->stream = stderr;
	else
		self->stream = fopen(self->c.filename, "a");

	if (self->stream == NULL)
		fprintf(stderr, "Failed to open frame output file '%s'", self->c.filename);

}

#endif