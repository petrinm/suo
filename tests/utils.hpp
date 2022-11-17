#pragma once

#include "suo.hpp"

namespace suo {

struct Stats {
	Stats();

	void clear();
	double ber() const;

	unsigned int frame_errors;
	unsigned int total_frames;
	unsigned int total_bit_errors;
	unsigned int total_bits;
};

std::ostream& operator<<(std::ostream& stream, const Stats& stats);;

inline Bit random_bit() { return (unsigned int)rand() & 1; }
inline Byte random_byte() { return (unsigned int)rand() & 0xff; }

void generate_noise(SampleVector& samples, float noise_std, unsigned int noise_samples);
void add_noise(SampleVector& samples, float noise_std);
void delay_signal(float delay, SampleVector& samples);

void count_bit_errors(Stats& stats, const Frame& transmit_frame, const Frame& received_frame);

};