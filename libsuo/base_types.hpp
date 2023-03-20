#pragma once

#include <cstdint>
#include <complex>
#include <vector>
#include <array>


using namespace std::complex_literals;

namespace suo
{

// Data type to represent I/Q samples in most calculations
typedef std::complex<float> Sample;
typedef std::complex<float> Complex;

typedef uint8_t Symbol;
typedef float SoftSymbol;


// Fixed-point I/Q samples
typedef uint8_t cu8_t[2];
typedef int16_t cs16_t[2];

// Data type to represent single bits. Contains a value 0 or 1.
typedef uint8_t Bit;
typedef uint8_t Byte;

/* Data type to represent soft decision bits.
    * 0 = very likely '0', 255 = very likely '1'.
    * Exact mapping to log-likelihood ratios is not specified yet. */
typedef uint8_t softbit_t;

typedef uint64_t Timestamp;

};
