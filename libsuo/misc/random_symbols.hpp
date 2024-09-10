#pragma once

#include <random>
#include "suo.hpp"
#include "generators.hpp"


namespace suo
{

/*
 */
class RandomSymbols : public Block
{
public:
	struct Config {
		Config();

		// Lenght of the burst in symbols
		unsigned int burst_length;

		//
		unsigned int delay_length;

		//
		unsigned int mode;

		// Complexity of the source symbol
		unsigned int complexity;
	};

	explicit RandomSymbols(const Config& conf = Config());

	RandomSymbols(const RandomSymbols&) = delete;
	RandomSymbols& operator=(const RandomSymbols&) = delete;

	/* */
	void reset();

	/* */
	SymbolGenerator generateSymbols(Timestamp now);


private:

	/* Coroutine for geneting symbol sequence */
	SymbolGenerator symbolGenerator();

	/* Configuration */
	Config conf;
};

}; // namespace suo
