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
		samples[i] += noise_std * Complex(randnf(), randnf()) * M_SQRT1_2f; // cawgn
		//cawgn(samples[i], noise_std);
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