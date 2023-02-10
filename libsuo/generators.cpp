#include <iostream>

#include "generators.hpp"


using namespace suo;
using namespace std;


extern const SymbolVector SymbolGenerator::invalid_vector(0);

SymbolGenerator::SymbolPromise::SymbolPromise() : 
	out(nullptr)
{
	input_iter = input_end = invalid_vector.end();
}

SymbolGenerator SymbolGenerator::SymbolPromise::get_return_object() {
	return SymbolGenerator(handle_type::from_promise(*this));
}

void SymbolGenerator::SymbolPromise::unhandled_exception() {
	// saving exception
	exception_ = std::current_exception();
}



std::suspend_always SymbolGenerator::SymbolPromise::yield_value(const Symbol& s)
{
	assert(out != nullptr);
	if (out->full())
		throw SuoError("Cannot append value! Output buffer is full.");

	//cout << "yield single: " << (int)s << endl;
	out->push_back(s);
	return { };
}


std::suspend_always SymbolGenerator::SymbolPromise::yield_value(const SymbolVector& symbols)
{
	assert(out != nullptr);
	//cout << "yield many: " << symbols.size() << " items" << endl;

	SymbolVector::const_iterator iter = symbols.begin();
	SymbolVector::const_iterator end = symbols.end();

	while (1) {

		// End of input vector
		if (iter == end)
			break;
		
		// Output vector is full
		if (out->full()) {
			input_iter = iter; // Preempt
			input_end = end;
			break;
		}

		out->push_back(*iter++);
	}

	return {};
}


std::suspend_always SymbolGenerator::SymbolPromise::yield_value(SymbolGenerator& gen)
{
	assert(out != nullptr);
	cout << "yield generator: " << endl;
	throw SuoError("Yielding a generator is not yet implemented!");
#if 0
	while (1) {
		// End of input generator
		gen.sourceSymbols(*out);

		// Output vector is full
		if (out->full()) {
			promise.gen = gen;
			break;
		}

		out->push_back(*iter++);
	}
#endif

	return { };
}


void SymbolGenerator::SymbolPromise::return_void() {
	//cout << "return void" << endl;
}

///////////////////////////////


SymbolGenerator::~SymbolGenerator() {
	if (coro_handle)
		coro_handle.destroy();
}


SymbolGenerator::SymbolGenerator(SymbolGenerator&& other) noexcept:
	coro_handle{ other.coro_handle }
{
	other.coro_handle = {};
}

SymbolGenerator& SymbolGenerator::operator=(SymbolGenerator&& other) noexcept {
	if (this != &other) {
		if (coro_handle)
			coro_handle.destroy();
		coro_handle = other.coro_handle;
		start = other.start;
		other.coro_handle = {};
		other.start = true;
	}
	return *this;
}


bool SymbolGenerator::running() const
{
	if (!coro_handle)
		return false;

	return !coro_handle.done();
}



void SymbolGenerator::sourceSymbols(SymbolVector& out)
{
	if (!coro_handle)
		throw SuoError("Cannot source samples from invalid generator");

	if (out.capacity() == 0)
		throw SuoError("out.capacity() == 0");
	out.clear();


	if (start) {
		start = false;
		out.flags |= start_of_burst;
	}

	SymbolPromise& promise = coro_handle.promise();

	/* Source samples from preempted iterators */
	if (promise.input_iter != promise.input_end) {
		while (1) {

			// End of input vector?
			if (promise.input_iter == promise.input_end)
				break;

			// Output vector is full?
			if (out.full())
				return;

			out.push_back(*promise.input_iter++);
		}

		// Make sure the iterators points to 'invalid_vector' because
		// the original source vector might get invalidated soon.
		promise.input_iter = promise.input_end = invalid_vector.end();
	}

#if 0
	/* Source samples from preempted generator */
	if (gen) {
		gen.sourceSamples(inserter);
	}
#endif

	/* Try to produce new samples */
	try {
		promise.out = &out;

		while (out.full() == false) {
			
			//cout << "generate_more " << out.size() << " < " << out.capacity() << endl;

			// Continue execution to generate more samples
			coro_handle();

			// Check exceptions
			if (promise.exception_)
				std::rethrow_exception(promise.exception_);

			// If coroutine returned mark the end of the burst
			if (coro_handle.done()) {
				//cout << "generator done" << endl;
				out.flags |= end_of_burst;
				break;
			}
		}
		
		promise.out = nullptr;
	}
	catch (...) {
		promise.out = nullptr;
		throw;
	}

}




////////////////////////////////////////////////////////////////////////////////

const SampleVector SampleGenerator::invalid_vector(0);


SampleGenerator::SamplePromise::SamplePromise() :
	out(nullptr)
{
	input_iter = input_end = invalid_vector.end();
}

SampleGenerator SampleGenerator::SamplePromise::get_return_object() {
	return SampleGenerator(handle_type::from_promise(*this));
}


void SampleGenerator::SamplePromise::unhandled_exception() {
	// saving exception
	exception_ = std::current_exception();
}


std::suspend_always SampleGenerator::SamplePromise::yield_value(const Sample& s)
{
	assert(out != nullptr);
	if (out->full())
		throw SuoError("Cannot append value! Output buffer is full.");

	//cout << "yield single: " << s << endl;
	out->push_back(s);
	return { };
}


std::suspend_always SampleGenerator::SamplePromise::yield_value(const SampleVector& samples)
{
	assert(out != nullptr);
	//cout << "yield many: " << samples.size() << " items" << endl;

	SampleVector::const_iterator iter = samples.begin();
	SampleVector::const_iterator end = samples.end();

	while (1) {

		// End of input vector
		if (iter == end)
			break;

		// Output vector is full
		if (out->full()) {
			input_iter = iter; // Preempt
			input_end = end;
			break;
		}

		out->push_back(*iter++);
	}

	return {};
}


void SampleGenerator::SamplePromise::return_void() {
	//cout << "return void" << endl;
}


SampleGenerator::~SampleGenerator() {
	if (coro_handle)
		coro_handle.destroy();
}


SampleGenerator::SampleGenerator(SampleGenerator&& other) noexcept : 
	coro_handle{ other.coro_handle }
{
	other.coro_handle = {};
}

SampleGenerator& SampleGenerator::operator=(SampleGenerator&& other) noexcept {
	if (this != &other) {
		if (coro_handle)
			coro_handle.destroy();
		coro_handle = other.coro_handle;
		start = other.start;
		other.coro_handle = {};
		other.start = true;
	}
	return *this;
}


//////////////////////////////// 

bool SampleGenerator::running() const
{
	if (!coro_handle)
		return false;
	return !coro_handle.done();
}


void SampleGenerator::sourceSamples(SampleVector& out)
{
	if (!coro_handle)
		throw SuoError("Cannot source samples from invalid generator");

	if (out.capacity() == 0)
		throw SuoError("out.capacity() == 0");
	out.clear();

	if (start) {
		start = false;
		out.flags |= start_of_burst;
	}


	SamplePromise& promise = coro_handle.promise();

	/* Source samples from preempted iterators */
	if (promise.input_iter != promise.input_end) {
		while (1) {

			// End of input vector?
			if (promise.input_iter == promise.input_end)
				break;

			// Output vector is full?
			if (out.full())
				return;

			out.push_back(*promise.input_iter++);
		}

		// Make sure the iterators points to 'invalid_vector' because
		// the original source vector might get invalidated soon.
		promise.input_iter = promise.input_end = invalid_vector.end();
	}

#if 0
	/* Source samples from preempted generator */
	if (gen) {
		gen.sourceSamples(inserter);
	}
#endif

	/* Try to produce new samples */
	try {
		promise.out = &out;

		while (out.full() == false) {
			//cout << "generate_more " << out.size() << " < " << out.capacity() << endl;

			// Continue execution to generate more samples
			coro_handle();

			// Check exceptions
			if (promise.exception_)
				std::rethrow_exception(promise.exception_);

			// If coroutine returned mark the end of the burst
			if (coro_handle.done()) {
				//cout << "generator done" << endl;
				out.flags |= end_of_burst;
				break;
			}
		}

		promise.out = nullptr;
	}
	catch (...) {
		promise.out = nullptr;
		throw;
	}
}

