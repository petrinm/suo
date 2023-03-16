#pragma once

#include "suo.hpp"
#include <ios>
#include <fstream>
#include <memory>

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

	explicit FileIO(const Config& conf = Config());
	//~FileIO();

	void execute();

	Port<const SampleVector&, Timestamp> sinkSamples;
	Port<SampleVector&, Timestamp> sourceSamples;

private:
	const Config conf;
	std::shared_ptr<std::istream> in;
	std::shared_ptr<std::ostream> out;
};


}; // namespace suo
