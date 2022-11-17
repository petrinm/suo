#pragma once

#include <string>
#include <vector>
#include <map>

#include "base_types.hpp"


namespace suo {



/* Metadata struct */
class Metadata
{
public:

	/* Metadata types */
	enum MetadataTypeType
	{
		METATYPE_RAW = 0,
		METATYPE_FLOAT = 1,
		METATYPE_DOUBLE = 2,
		METATYPE_INT = 3,
		METATYPE_UINT = 4,
		METATYPE_TIME = 5,
	};


	Metadata();
	void serialize();
	void deserialize();

	std::string name;

private:
	// Union for the actual data
	union {
		int32_t     v_int;
		uint32_t    v_uint;
		float       v_float;
		double      v_double;
		Timestamp   v_time;
		uint8_t     raw[16];
	} value;
};



/*
 * Frame together with metadata
 */
class Frame
{
public:

	enum class Flags : int {
		none,
		has_timestamp = 1,
		no_late = 2,
	};

	enum FormattingFlags : int
	{
		PrintCompact = 1,
		PrintData = 2,
		PrintMetadata = 4,
		PrintAltColor = 8,
		PrintColored = 16
	};

	static FormattingFlags default_formatting;
	static size_t printer_data_column_count;

	struct Printer {
		const Frame& frame;
		FormattingFlags flags;
	};

	/*
	 */
	explicit Frame(size_t data_len = 256);

	void clear();


	void setMetadata(std::string field_name, int val);
	void setMetadata(std::string field_name, unsigned int val);
	void setMetadata(std::string field_name, float val);
	void setMetadata(std::string field_name, double val);
	void setMetadata(std::string field_name, Timestamp val);

	Byte operator[](size_t _n) { return data[_n]; }
	Byte operator[](size_t _n) const { return data[_n]; }

	bool empty() const { return data.size() == 0; }
	size_t size() const { return data.size(); }
	size_t allocation() const { return data.capacity(); }
	const uint8_t* raw_ptr() const { return data.data(); }

	/*  */
	uint32_t id;
	Flags flags;
	Timestamp timestamp;


	/* Metadata vector  */
	std::vector<Metadata> metadata;

	/* Actual data (can be bytes, bits or softbits)*/
	ByteVector data;

	Printer operator()(FormattingFlags flags) const { return { *this, flags }; }

};


std::ostream& operator<<(std::ostream& stream, const Frame& frame);
std::ostream& operator<<(std::ostream& stream, const Frame::Printer& printer);


constexpr Frame::FormattingFlags operator|(Frame::FormattingFlags a, Frame::FormattingFlags b) noexcept {
	return static_cast<Frame::FormattingFlags>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr Frame::FormattingFlags operator&(Frame::FormattingFlags a, Frame::FormattingFlags b) noexcept {
	return static_cast<Frame::FormattingFlags>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr Frame::FormattingFlags operator~(Frame::FormattingFlags a) noexcept {
	return static_cast<Frame::FormattingFlags>(~static_cast<int>(a));
}




}; // namespace suo
