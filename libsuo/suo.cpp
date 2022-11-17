
#include "suo.hpp"
#include "registry.hpp"
#include "coloring.hpp"

#include <iomanip>
#include <ctime>
#include <cstdarg> // va_start, va_end


using namespace suo;
using namespace std;


unsigned int suo::rx_id_counter = 0;


SuoError::SuoError(const char* format, ...) : std::exception() {
	va_list args;
	va_start(args, format);
	vsnprintf(error_msg, 512, format, args);
	va_end(args);
}


Frame::FormattingFlags Frame::default_formatting = Frame::PrintData | Frame::PrintMetadata | Frame::PrintColored;

size_t Frame::printer_data_column_count = 32;


Frame::Frame(size_t data_len) :
	id(0),
	flags(Frame::Flags::none),
	timestamp(0)
{
	data.reserve(data_len);
}


void Frame::clear()
{
	id = 0;
	timestamp = 0;
	flags = Frame::Flags::none;
	metadata.clear();
	data.clear();
}


std::ostream& suo::operator<<(std::ostream &_stream, const Frame &frame) {
	_stream << frame(Frame::default_formatting);
	return _stream;
}

std::ostream& suo::operator<<(std::ostream& _stream, const Frame::Printer& printer)
{
	std::ostream stream(_stream.rdbuf());
	const Frame& frame = printer.frame;
	Frame::FormattingFlags flags = printer.flags;

	if (flags & Frame::PrintColored)
		stream << ((flags & Frame::PrintAltColor) ? clr::cyan : clr::green);

	if (flags & Frame::PrintCompact) {
		stream << left;
		stream << "Frame #" << setw(6) << frame.id << " "; // "% -6u";
		stream << "length: " << setw(3) << frame.data.size() << "bytes, ";
		stream << "timestamp: " << setw(10) << frame.timestamp; // "%-10lu\n",

		if (flags & Frame::PrintColored)
			stream << clr::reset; // Reset color modificator

		return _stream;
	}

	stream << endl << "####" << endl;

	stream << left;
	stream << "Frame #" << setw(6) << frame.id << " ";
	stream << "length: " << setw(1) << frame.data.size() << " bytes, ";
	stream << "timestamp: " << setw(10) << frame.timestamp << endl;


	if (flags & Frame::PrintMetadata) {
		stream << "Metadata:\n    ";
		if (frame.metadata.size() > 0) {
			for (const Metadata& meta : frame.metadata)
				stream << meta.name << "=" << 0;
		}
		else
			stream << "(none)";
		stream << "\n";
	}


	if (flags & Frame::PrintData) { // right << setw(2) << internal 
		stream << "Data:" << hex << right << setfill('0');
		for (unsigned int i = 0; i < frame.data.size(); i++) {
			if ((i % Frame::printer_data_column_count) == 0)
				stream << endl << "   ";
			stream << setw(2) << (int)frame.data[i] << " ";
		}
		stream << "\n";
	}

	if (flags & Frame::PrintColored)
		stream << clr::reset; // Reset color modificator

	return _stream;
}



std::map<std::string, MakeFunction> modules;

Registry::Registry(const char* name, MakeFunction func) {
	modules[name] = func;
}


std::ostream& suo::operator<<(std::ostream& stream, const SampleVector& v) {
	for (const auto& c: v)
		stream << c << endl;
	return stream;
}

std::ostream& suo::operator<<(std::ostream& stream, const SymbolVector& v) {
	for (const auto& c : v)
		stream << (int)c << endl;
	return stream;
}





void Frame::setMetadata(std::string type, int val) { }
void Frame::setMetadata(std::string type, unsigned int val) { }
void Frame::setMetadata(std::string type, float val) { }
void Frame::setMetadata(std::string type, double val) {}
void Frame::setMetadata(std::string type, Timestamp val) { }

void Block::sinkFrame(const Frame& frame) { }
void Block::sourceFrame(Frame& frame) { }

void Block::sinkSymbol(Symbol sym, Timestamp timestamp) { }
void Block::sinkSymbols(const std::vector<Symbol>& symbols, Timestamp timestamp) { }
void Block::sourceSymbols(SymbolVector& symbols, Timestamp timestamp) { }

void Block::sinkSoftSymbol(SoftSymbol sym, Timestamp timestamp) { }
void Block::sinkSoftSymbols(const std::vector<SoftSymbol>& sym, Timestamp timestamp) { }

void Block::sinkSamples(const SampleVector& samples, Timestamp timestamp) { }
void Block::sourceSamples(const SampleVector& samples, Timestamp timestamp) { }



Timestamp get_time() {
	timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		throw SuoError("clock_gettime: %d", errno);
	return 1000000 * ts.tv_sec + ts.tv_nsec;
}

