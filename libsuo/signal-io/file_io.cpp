#include "signal-io/file_io.hpp"
#include "signal-io/conversion.hpp"
#include "registry.hpp"
#include <SoapySDR/ConverterRegistry.hpp>

#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>


using namespace std;
using namespace suo;


struct noop {
	void operator()(...) const {}
};

FileIO::Config::Config() {
	format = "CF32";
	throttle = false;
	sample_rate = 100e3; // [Hz]
}


FileIO::FileIO(const Config& _conf) :
	conf(_conf)
{
	/* Setup input stream */
	if (conf.input == "-")
		in.reset(&cin, noop());
	else if (conf.input.empty() == false) {
		cout << "Opening '" << conf.input << "' for signal input." << endl;
		std::ifstream *input_file = new std::ifstream(conf.input);
		if (!*input_file)
			throw SuoError("Failed to open signal input file");
		in.reset(input_file);
	}

	/* Setup output stream */
	if (conf.output == "-")
		out.reset(&cout, noop());
	else if (conf.output.empty() == false) {
		cout << "Opening '" << conf.input << "' for signal output." << endl;
		std::ofstream *output_file = new std::ofstream(conf.output);
		if (!*output_file)
			throw SuoError("Failed to open signal output file");
		out.reset(output_file);
	}

	if ((in.get() != nullptr && in->bad()) && (out.get() != nullptr && out->bad()))
		throw SuoError("Neither to file input or output is defined");
}

void FileIO::execute()
{
	Timestamp now = 0;
	Timestamp tx_latency_time = 0;

	const size_t buffer_len = 4 * 4096;

	size_t input_format_size = SoapySDR::formatToSize(conf.format);
	SoapySDR::ConverterRegistry::ConverterFunction converter = nullptr;

	char* read_buffer = nullptr;
	SampleVector buffer(buffer_len);

	if (conf.format != "CF32") {
		converter = SoapySDR::ConverterRegistry::getFunction(conf.format, "CF32");
		if (converter == nullptr)
			throw SuoError("Unsupported input format %s", conf.format.c_str());
		read_buffer = new char[buffer_len * input_format_size];
	}
	else {
		read_buffer = reinterpret_cast<char*>(buffer.data());
	}

	unsigned int total_samples = 0;

	while (true) {

		size_t new_samples;

		// RX
		if (sinkSamples.has_connections() && in.get() != nullptr) {

			// Read more samples
			size_t read_len = in->readsome(read_buffer, input_format_size * buffer_len);
			if (read_len == 0) // TODO: Check for EOF not for read=0
				break;

			if ((read_len % input_format_size) != 0)
				throw SuoError("Number of read bytes is not diviable by the input format size!");
			new_samples = read_len / input_format_size;
			total_samples += new_samples;


			// Convert the samples to complex floats if needed
			if (converter != nullptr)
				converter(read_buffer, buffer.data(), new_samples, 1);
			buffer.resize(new_samples);
			
			// Feed
			sinkSamples.emit(buffer, now);
		}

#if 0
		// TX
		if (sourceSamples.has_connections()) {
			assert(n <= BUFLEN);
			buffer.clear();
			int tr = sourceSamples(buffer, n, now + tx_latency_time);
			ofstream.write(buffer.data(), buffer.size() * input_format_size);
		}
#endif

		Timestamp samples_ns = 1e9 * new_samples / conf.sample_rate;
		now += samples_ns;

		if (conf.throttle)
			std::this_thread::sleep_for(std::chrono::nanoseconds(samples_ns));
		
	}

	cout << "Processed " << total_samples << " samples in total";
	cout << " (" << fixed << setprecision(1) << (total_samples / conf.sample_rate) << " seconds)" << endl;
}


Block* createFileIO(const Kwargs& args)
{
	return new FileIO();
}

static Registry registerFileIO("FileIO", &createFileIO);
