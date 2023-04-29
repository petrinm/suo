#include <iostream>
#include <fstream>
#include <iomanip>

#include "file_dump.hpp"
#include "registry.hpp"


using namespace suo;
using namespace std;

FileDump::Config::Config() :
	format(FileFormatASCII)
{}

FileDump::FileDump(const Config& _conf) :
	conf(_conf),
	first_row(true)
{
	if (conf.filename.empty() == false)
		open(conf.filename);
}

FileDump::~FileDump() {
	close();
}

void FileDump::open(const std::string& filename) {

	if (output.is_open()) // && file not stdout 
		output.close();

	if (filename == "-") {
		throw SuoError("File output to stdout is not supported yet!");
	}

	ios_base::openmode flags = ios::out | ios::app;
	if (conf.format == FileFormatRaw || conf.format == FileFormatKISS)
		flags |= ios::binary;

	output.open(filename, flags);
	
	if (output.is_open() == false)
		throw SuoError("FileDump: Failed to open output file %s", filename.c_str());

	first_row = true;
}


void FileDump::close() {
	if (conf.format == FileFormatJSON)
		output << "\n]";
	output.close();
}


void FileDump::sinkFrame(const Frame& frame, Timestamp timestamp) {

	if (output.is_open() == false)
		throw SuoError("FileDump: No output stream opened!");

	switch (conf.format) {
	case FileFormatRaw: {
		/* Output as raw binary to file */
		output.write(reinterpret_cast<const char*>(frame.raw_ptr()), frame.size());
		break;
	}
	case FileFormatKISS: {
		/* Output as raw binary using KISS framing */
		const uint8_t FEND = 0xC0; // Frame End
		const uint8_t FESC = 0xDB; // Frame Escape
		const uint8_t TFEND = 0xDC; // Transposed Frame End
		const uint8_t TFESC = 0xDD; // Transposed Frame Escape
		const uint8_t CMD = 0x00;

		output << FEND << CMD;
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
	case FileFormatASCII: {
		/* Print frame using Suo's verbose ASCII formatting. */
		output << frame(Frame::PrintData | Frame::PrintMetadata);
		break;
	}
	case FileFormatASCIIHex: {
		/* Print frame data as hexadecimal string to file. */
		output << "# " << getCurrentISOTimestamp() << endl;
		output << hex << right << setw(2) << setfill('0');
		for (unsigned int i = 0; i < frame.data.size(); i++) {
			output << (int)frame.data[i];
		}
		output << endl;
		output.flush();
		break;
	}
	case FileFormatJSON: {
		/* Prinf frame as JSON dict */
		if (first_row) {
			output << "[\n";
			first_row = false;
		}
		else
			output << ",\n";
		output << frame.serialize_to_json();
		output.flush();
		break;
	}
	default:
		throw SuoError("FileDump: Invalid output format %d", conf.format);
	}
}
