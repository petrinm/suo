
#include <iostream>
#include <cstring> // memcpy

#include "suo.hpp"
#include "registry.hpp"
#include "modem/mod_gmsk.hpp"


using namespace suo;
using namespace std;



GMSKModulator::Config::Config() {
	sample_rate = 1e6;
	symbol_rate = 9600;
	center_frequency = 100e3;
	frequency_offset = 0.0f;
	bt = 0.5;
	amplitude = 1.0f;
	ramp_up_duration = 0;
	ramp_down_duration = 0;
}


GMSKModulator::GMSKModulator(const Config& conf) :
	conf(conf)
{

	sample_ns = round(1.0e9 / conf.sample_rate);
	nco_1Hz = pi2f / conf.sample_rate;

	const float samples_per_symbol = conf.sample_rate / conf.symbol_rate;
	mod_rate = (unsigned int)samples_per_symbol + 1;
	const float resamp_rate = samples_per_symbol / (float)mod_rate;

	if (mod_rate < 3)
		throw SuoError("GMSKModulator: mod_rate < 3");

	/* 
	 * Buffers
	 */
	symbols.reserve(64);

	/*
	 * Init GMSK modulator
	 */
	unsigned int gmsk_filter_delay = 2; // [symbols]
	l_mod = gmskmod_create(mod_rate, gmsk_filter_delay, conf.bt);

	/*
	 * Init NCO for up mixing
	 */
	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset));


	/*
	 * Init resampler
	 * The GMSK demodulator can produce signal samples/symbol ratio is an integer.
	 * This ratio doesn't usually match with the SDR's output sample ratio and thus fractional
	 * resampler is needed.
	 */
	l_resamp = resamp_crcf_create(resamp_rate,
		13,    /* filter semi-length (filter delay) */
		0.45f, /* resampling filter bandwidth */
		60.0f, /* resampling filter sidelobe suppression level */
		32     /* number of filters in bank (timing resolution) */
	);


	// Calculate number of trailer symbols to 
	trailer_length = gmsk_filter_delay + conf.ramp_down_duration;

	// Calculate total delay of the start of the transmission due to gaussian filter and resampling 
	filter_delay = ceil((gmsk_filter_delay * mod_rate + resamp_crcf_get_delay(l_resamp)) * sample_ns);

	reset();
}


GMSKModulator::~GMSKModulator()
{
	gmskmod_destroy(l_mod);
	resamp_crcf_destroy(l_resamp);
	nco_crcf_destroy(l_nco);
}

void GMSKModulator::reset()
{
	symbols.clear();

	gmskmod_reset(l_mod);
	nco_crcf_reset(l_nco);
	resamp_crcf_reset(l_resamp);
}


SampleGenerator GMSKModulator::generateSamples(Timestamp now)
{
	symbol_gen = generateSymbols.emit(now);
	if (symbol_gen.running())
		return sampleGenerator();
	return SampleGenerator();
}


SampleGenerator GMSKModulator::sampleGenerator()
{

#if 0
	/*
	 * Waiting for transmitting time
	 */
	if ((symbols.flags & has_timestamp)) {
		symbols.timestamp -= filter_delay;

		// If start time is defined and in history, rise a warning 
		if (symbols.timestamp < now) {
			int64_t late = (int64_t)(now - symbols.timestamp);
			if (symbols.flags & VectorFlags::no_late) {
				cerr << "Warning: TX frame late by " << late << "ns! Discarding it!" << endl;
				state = Idle;
				symbols.clear();
			}
			else
				cerr << "Warning: TX frame late by " << late << "ns" << endl;
		}

		// If start time is in future, don't start sample generation yet
		const Timestamp time_end = now + (Timestamp)(sample_ns * samples.capacity());
		if (symbols.timestamp > time_end)
			co_yield waiting;
	}
#endif

#ifdef PRECISE_TIMING
	// Generate zero samples for even more precise timing
	size_t silence_symbols = (size_t)floor((double)(now - symbols.timestamp) / sample_ns);
	if (silence_symbols > 0 && silence_symbols < samples.capacity()) {
		for (size_t i = 0; i < silence_symbols; i++)
			co_yield 0.0f;
	}
#endif

	// Update the mixer NCO on the correct frequency
	gmskmod_reset(l_mod);
	nco_crcf_set_phase(l_nco, 0);
	nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset));
	resamp_crcf_reset(l_resamp);

	SampleVector mod_samples;
	mod_samples.resize(mod_rate);


	/*
	 * Transmitting/generating samples from symbols
	 */
	while (1) {

		symbol_gen.sourceSymbols(symbols);
		if (symbols.empty())
			break;

		for (size_t si = 0; si < symbols.size(); si++) {
			Symbol symbol = symbols[si];

			// Generate samples from the symbol
			mod_samples.resize(mod_rate);
			gmskmod_modulate(l_mod, symbol, mod_samples.data());

			// Scale the signal by the amplitude
			liquid_vectorcf_mulscalar(mod_samples.data(), mod_rate, conf.amplitude, mod_samples.data());

			// Interpolate to final sample rate
			unsigned int num_written = 0;
			resamp_crcf_execute_block(l_resamp, mod_samples.data(), mod_rate, mod_samples.data(), &num_written);
			mod_samples.resize(num_written);

			// Mix up the samples
			nco_crcf_mix_block_up(l_nco, mod_samples.data(), mod_samples.data(), mod_samples.size());

			// Calculate amplitude ramp up
			if ((symbols.flags & start_of_burst) != 0 && si < conf.ramp_up_duration) {
				const size_t ramp_duration = conf.ramp_up_duration * mod_rate;
				const size_t hamming_window_start = si * mod_rate;
				const size_t hamming_window_len = 2 * ramp_duration;
				//cout << "Ramp up " << ramp_duration << " " << hamming_window_start << " " << hamming_window_len << endl;
				for (unsigned int i = 0; i < mod_samples.size(); i++)
					mod_samples[i] *= hamming(hamming_window_start + i, hamming_window_len); // TODO: liquid_hamming
			}
		
			co_yield mod_samples;
		}

		if (symbols.flags & end_of_burst)
			break;

		symbols.clear();
	}

	/*
	 * Generating random symbols to generate the postamble.
	 */
	for (size_t i = 0; i < trailer_length; i++) {

		// Feed zeros to the modulator to "flush" the gaussian filter
		mod_samples.resize(mod_rate);
		gmskmod_modulate(l_mod, 0, mod_samples.data());

		// Scale the signal by the amplitude
		liquid_vectorcf_mulscalar(mod_samples.data(), mod_rate, conf.amplitude, mod_samples.data());

		// Interpolate to final sample rate
		unsigned int resampler_output = 0;
		mod_samples.resize(mod_rate);
		resamp_crcf_execute_block(l_resamp, mod_samples.data(), mod_samples.size(), mod_samples.data(), &resampler_output);
		mod_samples.resize(resampler_output);

		// Mix up the samples
		nco_crcf_mix_block_up(l_nco, mod_samples.data(), mod_samples.data(), resampler_output);

		// Calculate amplitude ramp down
		if (conf.ramp_down_duration > 0) {
			int end = i - trailer_length + conf.ramp_down_duration - 1;
			if (end >= 0) {
				const size_t ramp_duration = conf.ramp_down_duration * mod_rate;
				const size_t hamming_window_start = (conf.ramp_down_duration + end) * mod_rate;
				const size_t hamming_window_len = 2 * ramp_duration;
				//cout << "Ramp down " << ramp_duration << " " << hamming_window_start << " " << hamming_window_len << " " << mod_rate << endl;
				for (unsigned int i = 0; i < mod_samples.size(); i++)
					mod_samples[i] *= hamming(hamming_window_start + i, hamming_window_len); // TODO: liquid_hamming
			}
		}
		
		co_yield mod_samples;
	}

	/*
	 *
	 */
	if (0) {
		// Prepare zeros for the resampler
		size_t resampler_delay = resamp_crcf_get_delay(l_resamp);
		mod_samples.resize(resampler_delay);
		for (size_t i = 0; i < resampler_delay; i++)
			mod_samples[i] = 0;

		// Interpolate to final sample rate
		unsigned int resampler_output = 0;
		mod_samples.resize(resampler_delay);
		resamp_crcf_execute_block(l_resamp, mod_samples.data(), resampler_delay, mod_samples.data(), &resampler_output);
		mod_samples.resize(resampler_output);

		// Mix up the samples
		nco_crcf_mix_block_up(l_nco, mod_samples.data(), mod_samples.data(), resampler_output);

		co_yield mod_samples;
	}

}

void GMSKModulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
}


Block* createGMSKModulator(const Kwargs& args)
{
	return new GMSKModulator();
}

static Registry registerGMSKModulator("GMSKModulator", &createGMSKModulator);

