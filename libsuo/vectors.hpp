#pragma once

#include "suo.hpp"
#include <cassert>

namespace suo {

enum VectorFlags
{
	none = 0,
	has_timestamp = 1,
	start_of_burst = 2,
	end_of_burst = 4,
	no_late = 8,
};


constexpr VectorFlags operator|(VectorFlags a, VectorFlags b) noexcept {
	return static_cast<VectorFlags>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr VectorFlags operator|=(VectorFlags& a, VectorFlags b) noexcept {
	return static_cast<VectorFlags>((int&)a |= static_cast<int>(b));
}

constexpr VectorFlags operator&(VectorFlags a, VectorFlags b) noexcept {
	return static_cast<VectorFlags>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr VectorFlags operator~(VectorFlags a) noexcept {
	return static_cast<VectorFlags>(~static_cast<int>(a));
}

//template<class T> inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); }

class SampleVector : public std::vector<Sample>
{
public:
	Timestamp timestamp;
	VectorFlags flags;

	void clear() {
		std::vector<Sample>::clear();
		flags = none;
		timestamp = 0;
	}
};
class SymbolVector : public std::vector<Symbol>
{
public:
	Timestamp timestamp;
	VectorFlags flags;
	
	void clear() {
		std::vector<Symbol>::clear();
		flags = none;
		timestamp = 0;
	}
};

typedef std::vector<unsigned char> ByteVector;

std::ostream& operator<<(std::ostream& stream, const SampleVector& v);
std::ostream& operator<<(std::ostream& stream, const SymbolVector& v);

//std::ostream& suo::operator<<(std::ostream& _stream, const BitVector& v);
//std::ostream& suo::operator<<(std::ostream& _stream, const ByteVector& v);

}; // namespace suo