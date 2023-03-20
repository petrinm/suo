#pragma once

#include <string>
#include <vector>
#include <variant>
#include <map>

#include "base_types.hpp"

namespace suo {


typedef std::variant<int, unsigned int, float, double, Timestamp, std::string> MetadataValue;
typedef std::pair<std::string, MetadataValue> Metadata;

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
		control_frame = 4,
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

	template<typename T>
	void setMetadata(std::string field_name, T val) {
		metadata[field_name] = val;
	}

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
	std::map<std::string, MetadataValue> metadata;

	/* Actual data (can be bytes, bits or softbits)*/
	ByteVector data;

	Printer operator()(FormattingFlags flags) const { return { *this, flags }; }

	//static SymbolGenerator generateSymbols(Frame& frame);

	static Frame deserialize_from_json(const std::string& json_string);
	std::string serialize_to_json() const;

};


constexpr Frame::Flags operator|(Frame::Flags a, Frame::Flags b) noexcept {
	return static_cast<Frame::Flags>(static_cast<int>(a) | static_cast<int>(b));
}

inline Frame::Flags& operator|=(Frame::Flags& a, Frame::Flags b) noexcept {
	a = static_cast<Frame::Flags>(static_cast<int>(a) | static_cast<int>(b));
	return a;
}

constexpr Frame::Flags operator&(Frame::Flags a, Frame::Flags b) noexcept {
	return static_cast<Frame::Flags>(static_cast<int>(a) & static_cast<int>(b));
}

//constexpr  operator bool(Frame::Flags a) noexcept {
//	return static_cast<int>(a) != 0;
//}

inline Frame::Flags& operator&=(Frame::Flags& a, Frame::Flags b) noexcept {
	a = static_cast<Frame::Flags>(static_cast<int>(a) & static_cast<int>(b));
	return a;
}

constexpr Frame::Flags operator~(Frame::Flags a) noexcept {
	return static_cast<Frame::Flags>(~static_cast<int>(a));
}


std::ostream& operator<<(std::ostream& stream, const Metadata& metadata);

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
