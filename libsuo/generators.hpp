#pragma once

#include "suo.hpp"
#include "generators.hpp"

#include <coroutine>
#include <iterator>

namespace suo
{

// template<typename Complexity>
class SymbolGenerator
{
public:
	//const unsigned int complexity = Complexity;

	class SymbolPromise {
	public:
		SymbolPromise();

		SymbolGenerator get_return_object();

		std::suspend_always initial_suspend() { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }

		void unhandled_exception();

		std::suspend_always yield_value(const Symbol& symbol);
		std::suspend_always yield_value(const SymbolVector& symbols);
		std::suspend_always yield_value(SymbolGenerator& gen);

		void return_void();

	private:

		SymbolVector* out;
		SymbolVector::const_iterator input_iter, input_end;
		std::exception_ptr exception_;
		
		friend SymbolGenerator;
	};


	using promise_type = SymbolPromise;
	using handle_type = std::coroutine_handle<promise_type>;

	SymbolGenerator() = default;
	SymbolGenerator(const SymbolGenerator&) = delete;
	SymbolGenerator(SymbolGenerator&& other) noexcept;
	SymbolGenerator(handle_type _handle) : coro_handle(_handle) { }

	~SymbolGenerator();

	SymbolGenerator& operator=(const SymbolGenerator&) = delete;
	SymbolGenerator& operator=(SymbolGenerator&& other) noexcept;

	/* Return done */
	bool running() const;

	/* Source new symbols from the generator */
	void sourceSymbols(SymbolVector& out);
	void sourceSymbols(std::insert_iterator<SymbolVector> inserter);

	operator bool() { return running(); }

	// Range-based for loop support.
	class Iterator {
	public:
		void operator++();
		Symbol operator*();
		bool operator==(std::default_sentinel_t) const;
		Iterator(SymbolGenerator& gen, size_t buffer_size);

	private:
		SymbolGenerator& gen;
		SymbolVector buffer;
		SymbolVector::iterator it;
	};

	/* */
	Iterator begin(size_t buffer_size=64);

	/* */
	std::default_sentinel_t end() { return {}; }

private:
	handle_type coro_handle;
	bool start = true;

	static const SymbolVector invalid_vector;

};
























class SampleGenerator
{
public:

	class SamplePromise {
	public:

		SamplePromise();

		SampleGenerator get_return_object();

		std::suspend_always initial_suspend() { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }

		void unhandled_exception();

		std::suspend_always yield_value(const Sample& s);
		std::suspend_always yield_value(const SampleVector& samples);
		std::suspend_always yield_value(const SampleGenerator& gen);

		void return_void();

	private:

		SampleVector* out;
		SampleVector::const_iterator input_iter, input_end;
		std::exception_ptr exception_;

		friend SampleGenerator;
	};


	using promise_type = SamplePromise;
	using handle_type = std::coroutine_handle<promise_type>;

	SampleGenerator() = default;
	SampleGenerator(const SampleGenerator&) = delete;
	SampleGenerator(SampleGenerator&& other) noexcept;
	SampleGenerator(handle_type _handle): coro_handle(_handle) { }

	~SampleGenerator();

	SampleGenerator& operator=(const SampleGenerator&) = delete;
	SampleGenerator& operator=(SampleGenerator&& other) noexcept;

	bool running() const;

	void sourceSamples(SampleVector& out);
	
	operator bool() { return running(); }

private:
	handle_type coro_handle;
	bool start = true;

	static const SampleVector invalid_vector;
	
};





/* */
class SymbolGeneratorFromCallback
{
public:
	typedef std::function<void(SymbolVector&, Timestamp)> CallbackType;

	/* */
	SymbolGeneratorFromCallback(CallbackType const& callback, size_t size);

	/* */
	SymbolGenerator generateSymbols(Timestamp now);

private:

	/* */
	SymbolGenerator generator();

	CallbackType const& callback;
	SymbolGenerator gen;
	SymbolVector symbols;
	Timestamp _now;
};


SymbolGenerator generator_from_vector(const SymbolVector& symbols);
SampleGenerator generator_from_vector(const SampleVector& samples);


}; // namespace suo
