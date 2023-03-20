#pragma once

#include "suo.hpp"
#include <fstream>
#include <random>

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


class EbN0Log
{
public:
	explicit EbN0Log(const std::string& name);
	void push_results(float snr, const Stats& stats);
private:
	std::ofstream file;
};



class RandomFrameGenerator {
public:
	explicit RandomFrameGenerator(unsigned int frame_len);
	RandomFrameGenerator(unsigned int frame_min_len, unsigned int frame_max_len);
	void set_seed(unsigned int seed);
	void source_frame(Frame& frame, Timestamp _now);
	void set_release_time(Timestamp time);
	void set_verbose(bool v);
	void set_frame_count(int c);

	const Frame& latest_frame() const { return frame; } 

private:
	std::mt19937 random_generator;
	unsigned int frame_min_len, frame_max_len;
	int frame_count;
	Timestamp release_time;
	Frame frame;
	bool verbose;
};


};