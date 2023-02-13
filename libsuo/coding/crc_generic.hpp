#pragma once

#include "suo.hpp"
#include "coding/crc_algorithms.hpp"
#include "framing/utils.hpp"

#include <array>
#include <iomanip>

namespace suo
{

/*
 * Type generic implementation of the CRC
 */
template<typename T, unsigned int Width>
class CRCGeneric
{
public:
	using TableType = std::array<T, 256>;
	using CacheType = std::map<const CRCAlgorithm*, TableType>;
	static const unsigned int TypeWidth = 8 * sizeof(T);


	class Digest {
	public:
		Digest(const CRCAlgorithm& algo): crc(algo), value(crc.init()) { }
		Digest(const CRCAlgorithm& algo, T initial): crc(algo), value(crc.init(initial)) { }
		void update(const ByteVector& bytes) { value = crc.update(value, bytes); };
		T finalize() { return crc.finalize(value); }
	private:
		CRCGeneric crc;
		T value;
	};

	/* Initialize generic CRC implementation */
	explicit CRCGeneric(const CRCAlgorithm& algo)
		: algo(algo), table(getTable(algo)) { }

	/* */
	T init() const {
		if (algo.refIn)
			return reverse_bits((T)algo.init) >> (TypeWidth - Width);
		else
			return algo.init << (TypeWidth - Width);
	}

	/* */
	T init(T initial) const {
		if (algo.refIn)
			return reverse_bits(initial) >> (TypeWidth - Width);
		else
			return initial << (TypeWidth - Width);
	}

	/* Update routine for ByteVector */
	T update(T value, const ByteVector& bytes) const {
		return update(value, bytes.data(), bytes.size());
	}

	/* Update routine for uint8_t pointer. */
	T update(T value, const uint8_t* bytes, size_t len) const {
		if (algo.refIn) {
			for (size_t i = 0; i < len; i++) {
				T index = value ^ bytes[i];
				value = table[index & 0xFF] ^ (value >> 8);
			}
		}
		else {
			for (size_t i = 0; i < len; i++) {
				T index = (value >> (Width - 8)) ^ bytes[i];
				value = table[index & 0xFF] ^ (value << 8);
			}
		}
		return value;
	}

	/* */
	T finalize(T value) const {
		if (algo.refIn ^ algo.refOut)
			value = reverse_bits(value);
		if (!algo.refOut)
			value >>= TypeWidth - algo.width;
		return value ^ algo.xorOut;
	}

	T calculate(const ByteVector& bytes) const {
		T x = finalize(update(init(), bytes));
		return x;
	}

	T calculate(const uint8_t* bytes, size_t len) const {
		return finalize(update(init(), bytes, len));
	}


	/* Printout the lookup table as C table definition. */
	void print_lookup_table(std::ostream& _stream, const std::string& name = "lookup_table") const {
		std::ostream stream(_stream.rdbuf());
		const size_t digits = Width / 4;
		const size_t column_count = 8; // (80 - 4) / (digits + 3);
		stream << "const uint8_t " << name << "[" << table.size() << "] = {";
		stream << std::setfill('0') << std::right << std::hex;
		for (size_t i = 0; i < table.size(); i++) {
			stream << ((i % column_count) == 0 ? "\n\t" : " ");
			stream << "0x" << std::setw(digits) << table[i] << ",";
		}
		stream << "\n};\n";
	}

private:
	const CRCAlgorithm& algo;
	const TableType& table;

	static CacheType cache;

	static const TableType& getTable(const CRCAlgorithm& algo) {
		
		assert(sizeof(TableType) >= Width);
		assert(algo.width == Width);

		/* Try to find the lookup table from the cache */
		auto iter = cache.find(&algo);
		if (iter != cache.end())
			return iter->second;
		

		/* Generate new lookup table */
		TableType& new_table = cache[&algo];
		const T poly = algo.refIn ?
			(reverse_bits((T)algo.poly) >> (TypeWidth - Width)) :
			(algo.poly << (TypeWidth - Width));

		for (unsigned int i = 0; i < 256; i++) {
			T value = i;
			if (algo.refIn) {
				for (unsigned int j = 0; j < 8; j++)
					value = (value >> 1) ^ ((value & 1) * poly);
			}
			else {
				value <<= (TypeWidth - 8);
				for (unsigned int j = 0; j < 8; j++)
					value = (value << 1) ^ (((value >> (Width - 1)) & 1) * poly);
			}
			new_table[i] = value;
		}

		return new_table;
	}

};

typedef CRCGeneric<uint8_t, 8> CRC8;
typedef CRCGeneric<uint16_t, 16> CRC16;
typedef CRCGeneric<uint32_t, 24> CRC24;
typedef CRCGeneric<uint32_t, 32> CRC32;
typedef CRCGeneric<uint64_t, 64> CRC64;

}; // namespace suo
