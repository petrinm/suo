#pragma once

#include "suo.hpp"

namespace suo {

/*
 */
class FileIO: public Block
{
public:
	
	/* 
	 */
	struct Config {
		Config();

		/* Sample rate of the files */
		double sample_rate;
		
		/* Throttle execution */
		bool throttle;

		/* File name of input file containing received signal */
		std::string input;

		/* File name for output file containing transmitted signal */
		std::string output;

		/* Data format */
		std::string format;
	};

	FileIO(const Config& conf = Config());
	~FileIO();

	void execute();

	Port<const SampleVector&, Timestamp> sinkSamples;
	Port<SampleVector&, Timestamp> sourceSamples;

private:
	Config conf;
	FILE* in, * out;
};

}; // namespace suo