#include "utils.hpp"
#include <iostream>
#include <liquid/liquid.h>

using namespace std;
using namespace suo;


Stats::Stats() {
	clear();
}

void Stats::clear() {
	frame_errors = 0;
	total_frames = 0;
	total_bit_errors = 0;
	total_bits = 0;
}

double Stats::ber() const {
	return ((double)total_bit_errors / (double)total_bits);
}

std::ostream& suo::operator<<(std::ostream& _stream, const Stats& stats) {
	std::ostream stream(_stream.rdbuf());
	stream << "    Total frames: " << stats.total_frames << endl;
	stream << "Total bit errors: " << stats.total_bit_errors << endl;
	stream << "      Total bits: " << stats.total_bits << endl;
	stream << "             BER: " << scientific << stats.ber() << endl;
	return _stream;
} 


void suo::generate_noise(SampleVector& samples, float noise_std, unsigned int noise_samples) {
	samples.clear();
	samples.resize(noise_samples);
	for (unsigned i = 0; i < noise_samples; i++)
		cawgn(&samples[i], noise_std);
}

void suo::add_noise(SampleVector& samples, float noise_std) {
	for (unsigned i = 0; i < samples.size(); i++)
		samples[i] += noise_std * Complex(randnf(), randnf()) * M_SQRT1_2f;
}

void suo::delay_signal(float delay, SampleVector& samples)
{
	unsigned int h_len = 19;   // filter length
	unsigned int p = 5;	       // polynomial order
	float	     fc = 0.45f;   // filter cutoff
	float        As = 60.0f;   // stop-band attenuation [dB]

	firfarrow_crcf f = firfarrow_crcf_create(h_len, p, fc, As);
	firfarrow_crcf_set_delay(f, delay);

	for (unsigned int i = 0; i < samples.size(); i++) {
		firfarrow_crcf_push(f, samples[i]);
		firfarrow_crcf_execute(f, &samples[i]);
	}

	firfarrow_crcf_destroy(f);
}


void suo::count_bit_errors(Stats& stats, const Frame& transmit_frame, const Frame& received_frame)
{
	unsigned int bit_errors = 0;
	size_t data_len = min(transmit_frame.size(), received_frame.size());
	for (unsigned int i = 0; i < data_len; i++)
		bit_errors += __builtin_popcountll(transmit_frame[i] ^ received_frame[i]);
	stats.total_bit_errors += bit_errors;
	stats.total_bits += 8 * data_len;
}



EbN0Log::EbN0Log(const std::string& name) {
	file.open(name + ".ebn0");
}

void EbN0Log::push_results(float snr, const Stats& stats) {
	file << snr << " ";
	file << stats.frame_errors << " ";
	file << stats.total_frames << " ";
	file << stats.total_bit_errors << " ";
	file << stats.total_bits << " ";
	file << std::endl;
	file.flush();
}


RandomFrameGenerator::RandomFrameGenerator(unsigned int frame_len) :
	random_generator(time(nullptr)),
	frame_min_len(frame_len),
	frame_max_len(frame_len),
	frame_count(-1),
	release_time(0),
	verbose(false)
{ }

RandomFrameGenerator::RandomFrameGenerator(unsigned int frame_min_len, unsigned int frame_max_len) :
	random_generator(time(nullptr)),
	frame_min_len(frame_min_len),
	frame_max_len(frame_max_len),
	frame_count(-1),
	release_time(0),
	verbose(false)
{ }

void RandomFrameGenerator::set_seed(unsigned int seed) {
	random_generator.seed(seed);
}

void RandomFrameGenerator::set_release_time(Timestamp time) {
	release_time = time;
}

void RandomFrameGenerator::set_verbose(bool v) {
	verbose = v;
}

void RandomFrameGenerator::set_frame_count(int c) {
	frame_count = c;
}


void RandomFrameGenerator::source_frame(Frame& frame_out, Timestamp now) {

	if (now < release_time)
		return;

	if (frame_count == 0)
		return;
	if (frame_count > 0)
		frame_count--;

	std::uniform_int_distribution<> frame_len_distrib(frame_min_len, frame_max_len);
	std::uniform_int_distribution<> byte_distrib(0, 255);

	unsigned int frame_len = frame_len_distrib(random_generator);

	frame.clear();
	frame.timestamp = now;
	frame.data.resize(frame_len);
	for (size_t i = 0; i < frame_len; i++)
		frame.data[i] = byte_distrib(random_generator);

	if (verbose)
		cout << frame(Frame::PrintData | Frame::PrintColored);
	frame_out = frame;
}
