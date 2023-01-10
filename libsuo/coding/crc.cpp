
#include "coding/crc.hpp"
#include "coding/crc_generic.hpp"

using namespace suo;

CRC::CRC(const CRCAlgorithm& algo) :
	algo(algo)
{ }

uint32_t CRC::init() const {
	switch (algo.width) {
	case 8: return CRCGeneric<uint8_t, 8>(algo).init();
	case 16: return CRCGeneric<uint16_t, 16>(algo).init();
	case 32: return CRCGeneric<uint32_t, 32>(algo).init();
	default: return 0;
	}
}

uint32_t CRC::init(uint32_t initial) const {
	return 0;
}

uint32_t CRC::update(uint32_t crc, const ByteVector& bytes) const {
	switch (algo.width) {
	case 8: return CRCGeneric<uint8_t, 8>(algo).update(crc, bytes);
	case 16: return CRCGeneric<uint16_t, 16>(algo).update(crc, bytes);
	case 32: return CRCGeneric<uint32_t, 32>(algo).update(crc, bytes);
	default: return 0;
	}
}

uint32_t CRC::finalize(uint32_t crc) const {
	switch (algo.width) {
	case 8: return CRCGeneric<uint8_t, 8>(algo).finalize(crc);
	case 16: return CRCGeneric<uint16_t, 16>(algo).finalize(crc);
	case 32: return CRCGeneric<uint32_t, 32>(algo).finalize(crc);
	default: return 0;
	}
}


// Initialize static CRC caches
template<> CRC8::CacheType CRC8::cache = {};
template<> CRC16::CacheType CRC16::cache = {};
template<> CRC24::CacheType CRC24::cache = {};
template<> CRC32::CacheType CRC32::cache = {};


/*
 * ref: https://reveng.sourceforge.io/crc-catalogue/all.htm
 */
namespace suo::CRCAlgorithms {

const CRCAlgorithm CRC8 = {
	.width = 8,
	.poly = 0x07,
	.init = 0x00,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x000,
};

const CRCAlgorithm CRC8_CDMA2000 = {
	.width = 8,
	.poly = 0x9B,
	.init = 0xFF,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x000,
};

const CRCAlgorithm CRC8_DVB_S2 = {
	.width = 8,
	.poly = 0xD5,
	.init = 0x00,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x000,
};

const CRCAlgorithm CRC8_ITU = {
	.width = 8,
	.poly = 0x07,
	.init = 0x00,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x55,
};

const CRCAlgorithm CRC16_AUG_CCITT = {
	.width = 16,
	.poly = 0x1021,
	.init = 0x1D0F,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x000,
};

const CRCAlgorithm CRC16_CCITT_FALSE = {
	.width = 16,
	.poly = 0x1021,
	.init = 0xFFFF,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x000,
};

const CRCAlgorithm CRC16_CDMA2000 = {
	.width = 16,
	.poly = 0xC867,
	.init = 0xFFFF,
	.refIn = false,
	.refOut = false,
	.xorOut = 0x000,
};

const CRCAlgorithm CRC16_X25 = {
	.width = 16,
	.poly = 0x1021,
	.init = 0xFFFF,
	.refIn = true,
	.refOut = true,
	.xorOut = 0xFFFF,
};

const CRCAlgorithm CRC24_BLE = {
	.width = 24,
	.poly = 0x1021,
	.init = 0xFFFF,
	.refIn = true,
	.refOut = true,
	.xorOut = 0xFFFF,
};

const CRCAlgorithm CRC32 = {
	.width = 32,
	.poly = 0x04C11DB7,
	.init = 0xFFFFFFFF,
	.refIn = true,
	.refOut = true,
	.xorOut = 0xFFFFFFFF,
};

const CRCAlgorithm CRC32_POSIX = {
	.width = 32,
	.poly = 0x04C11DB7,
	.init = 0x00000000,
	.refIn = false,
	.refOut = false,
	.xorOut = 0xFFFFFFFF,
};


using namespace std::literals::string_view_literals;
constexpr std::array<std::pair<std::string_view, const CRCAlgorithm&>, 10> algorithms{ {
	{ "CRC-8"sv, CRC8 },
	{ "CRC-8/CDMA2000"sv, CRC8_CDMA2000 },
	{ "CRC-8/DVB-S2"sv, CRC8_DVB_S2 },
	{ "CRC-8/ITU"sv, CRC8_ITU },

	{ "CRC-16/AUG-CCITT"sv, CRC16_AUG_CCITT },
	{ "CRC-16/CCITT_FALSE"sv, CRC16_CCITT_FALSE },
	{ "CRC-16/CDMA2000"sv, CRC16_CDMA2000 },
	{ "CRC-16/X25"sv, CRC16_X25 },

	{ "CRC-32"sv, CRC32 },
	{ "CRC-32/POSIX"sv, CRC32_POSIX },
} };

}; // namespace suo::CRCAlgorithms


const CRCAlgorithm& suo::getCRC(const std::string_view name) {
	static const auto map = std::map<std::string_view, const CRCAlgorithm&>{ 
		CRCAlgorithms::algorithms.begin(), CRCAlgorithms::algorithms.end() };
	try {
		return map.at(name);
	}
	catch (std::out_of_range& e) {
		throw SuoError("No such CRC code '%s'", name);
	}
}



