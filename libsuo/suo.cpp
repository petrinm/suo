
#include "suo.hpp"
#include "registry.hpp"
#include "coloring.hpp"

#include <iomanip>
#include <ctime>
#include <cstdarg> // va_start, va_end
#include <sys/time.h>



using namespace suo;
using namespace std;


unsigned int suo::rx_id_counter = 0;


SuoError::SuoError(const char* format, ...) : std::exception() {
	va_list args;
	va_start(args, format);
	vsnprintf(error_msg, 512, format, args);
	va_end(args);
}


std::map<std::string, MakeFunction> modules;

Registry::Registry(const char* name, MakeFunction func) {
	modules[name] = func;
}


std::ostream& suo::operator<<(std::ostream& stream, const SampleVector& v) {
	for (const auto& c: v)
		stream << c << " ";
	return stream;
}

std::ostream& suo::operator<<(std::ostream& stream, const SymbolVector& v) {
	for (const auto& c : v)
		stream << (int)c << " ";
	return stream;
}

std::ostream& suo::operator<<(std::ostream& _stream, const ByteVector& v) {
	std::ostream stream(_stream.rdbuf());
	stream <<  hex << right << setfill('0');
	for (const auto& c : v)
		stream << setw(2) << (int)c << " ";
	return _stream;
}


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


string suo::getISOCurrentTimestamp()
{
#if 1
	timeval curTime;
	gettimeofday(&curTime, NULL);

	int milli = curTime.tv_usec / 1000;
	char buf[64];
	char* p = buf + strftime(buf, sizeof buf, "%FT%T", gmtime(&curTime.tv_sec));
	sprintf(p, ".%dZ", milli);

	return buf;
#else
	auto now = chrono::system_clock::now();
	return std::format("{:%FT%TZ}", now);
#endif
}
