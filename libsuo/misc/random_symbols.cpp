#include <iostream>

#include "misc/random_symbols.hpp"
#include "framing/utils.hpp"
#include "registry.hpp"



using namespace suo;
using namespace std;



RandomSymbols::Config::Config() {
	burst_length = 1000;
	mode = 0;
	complexity = 2;
}

RandomSymbols::RandomSymbols(const Config& conf) :
	conf(conf)
{
	reset();
}

void RandomSymbols::reset() {
	//if (symbol_gen.running())
	//	symbol_gen.cancel();
}

SymbolGenerator RandomSymbols::generateSymbols(Timestamp now) {
	// TODO: Implemented periodical bursts
	return symbolGenerator();
}


SymbolGenerator RandomSymbols::symbolGenerator()
{
	if (conf.mode == 0) {
		/*
		 * Source random symbols
		 */
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distrib(0, conf.complexity - 1);

		for (unsigned int i = 0; i < conf.burst_length; i++) {
			co_yield distrib(gen);
		}
	}
	else {
		/*
		 * Source constant 0 symbol
		 */
		for (unsigned int i = 0; i < conf.burst_length; i++)
			co_yield 0;
	}

}


Block* createRandomSymbols(const Kwargs& args)
{
	return new RandomSymbols();
}

static Registry registerRandomSymbols("RandomSymbols", &createRandomSymbols);
