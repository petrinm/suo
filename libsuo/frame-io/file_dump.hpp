#pragma once

#include <string>

#include "suo.hpp"

namespace suo {

/**
 * @brief 
 * 
 */
class FileDump {
public:

	enum DumpFormat {
		FormatRaw,
		FormatKISS,
		FormatASCII,
		FormatASCIIHex,
	};

	struct Config {
		Config();

		DumpFormat format;
		std::string filename;
	};

	explicit FileDump(const Config& config = Config());

	void open(const std::string& filename);
	void close();

	void sinkFrame(const Frame& frame, Timestamp timestamp);

private:
	Config conf;
	std::ofstream output;
};

}; // namespace suo
