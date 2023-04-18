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
		FileFormatRaw,
		FileFormatKISS,
		FileFormatASCII,
		FileFormatASCIIHex,
		FileFormatJSON,
	};

	struct Config {
		Config();

		DumpFormat format;
		std::string filename;
	};

	explicit FileDump(const Config& config = Config());
	~FileDump();

	void open(const std::string& filename);
	void close();

	void sinkFrame(const Frame& frame, Timestamp timestamp);

private:
	Config conf;
	std::ofstream output;
	bool first_row;
};

}; // namespace suo
