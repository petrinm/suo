#include <iostream>

#include "modem/mod_psk.hpp"
#include "registry.hpp"

using namespace std;
using namespace suo;


PSKModulator::Config::Config() {
	sample_rate = 1e6f;
	symbol_rate = 9600.f;
	center_frequency = 100e3f;
	frequency_offset = 0.0f;
	amplitude = 1.0f;
	ramp_up_duration = 0;
	ramp_down_duration = 0;
}


PSKModulator::PSKModulator(const Config &_conf):
	conf(_conf)
{

	sample_ns = round(1.0e9 / conf.sample_rate);
	nco_1Hz = pi2f / conf.sample_rate;

	const float samples_per_symbol = conf.sample_rate / conf.symbol_rate;
	mod_rate = (unsigned int)samples_per_symbol + 1;
	const float resamp_rate = samples_per_symbol / (float)mod_rate;


	float signal_bandwidth = conf.symbol_rate; // [Hz]   For BPSK!!!
	if ((abs(conf.center_frequency) + 0.5 * signal_bandwidth) / conf.sample_rate > 0.5)
		throw SuoError("PSKModulator: Center frequency too large for given sample rate!");

	/*
	 * Buffers
	 */
	symbols.reserve(0x900);


	/* Init liquid modem object to create complex symbols from bits */ 
	l_mod = modemcf_create(LIQUID_MODEM_PSK2);

	/*
	 * Init NCO for carrier generation/up mixing
	 */
	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset) / resamp_rate); //  

	/*
	 * Init resampler
	 * The GMSK demodulator can produce signal samples/symbol ratio is an integer.
	 * This ratio doesn't usually match with the SDR's output sample ratio and thus fractional
	 * resampler is needed.
	 */
	l_resamp = resamp_crcf_create(resamp_rate,
		6,     /* filter semi-length (filter delay) */
		0.45f, /* resampling filter bandwidth */
		60.0f, /* resampling filter sidelobe suppression level */
		32     /* number of filters in bank (timing resolution) */
	);

	reset();
}


PSKModulator::~PSKModulator()
{
	modem_destroy(l_mod);
	nco_crcf_destroy(l_nco);
	resamp_crcf_destroy(l_resamp);
}

void PSKModulator::reset() {
	symbols.clear();

	modemcf_reset(l_mod);
	nco_crcf_reset(l_nco);
	resamp_crcf_reset(l_resamp);
}


SampleGenerator PSKModulator::generateSamples(Timestamp now) {
	symbol_gen = generateSymbols.emit(now);
	if (symbol_gen.running())
		return sampleGenerator();
	return SampleGenerator();
}


SampleGenerator PSKModulator::sampleGenerator()
{

#if 0
	/*
	 * Waiting for transmitting time
	 */
	if ((symbols.flags & has_timestamp)) {
			// If start time is defined and in history, rise a warning 
		if (symbols.timestamp < now) {
			int64_t late = (int64_t)(now - symbols.timestamp);
			if (symbols.flags & VectorFlags::no_late) {
				cerr << "Warning: TX frame late by " << late << "ns! Discarding it!" << endl;
				state = Idle;
				symbols.clear();
				return;
			}
			else
				cerr << "Warning: TX frame late by " << late << "ns" << endl;
		}

		// If start time is in future, don't start sample generation yet
		const Timestamp time_end = now + (Timestamp)(sample_ns * samples.capacity());
		if (symbols.timestamp > time_end)
			return;
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

	SampleVector mod_samples;

	// Update the mixer NCO on the correct frequency
	modemcf_reset(l_mod);
	nco_crcf_reset(l_nco);
	const float resamp_rate = resamp_crcf_get_rate(l_resamp);
	nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset) / resamp_rate);
	resamp_crcf_reset(l_resamp);

	/*
	 * Transmitting/generating samples from symbols
	 */
	while (symbol_gen.running()) {

		symbol_gen.sourceSymbols(symbols);
		if (symbols.empty())
			break;

		for (size_t si = 0; si < symbols.size(); si++) {

			// Calculate complex symbol from integer symbol
			Complex complex_symbol;
			modemcf_modulate(l_mod, symbols[si], &complex_symbol);
			complex_symbol *= conf.amplitude;

			// Mix up the samples
			Complex carrier;
			mod_samples.resize(mod_rate);
			for (unsigned int i = 0; i < mod_rate; i++) {
				// Generate carrier
				nco_crcf_step(l_nco);
				nco_crcf_cexpf(l_nco, &carrier);

				// Mix up the complex symbol
				mod_samples[i] = complex_symbol * carrier;
			}

			// Interpolate to final sample rate
			unsigned int num_written = 0;
			resamp_crcf_execute_block(l_resamp, mod_samples.data(), mod_samples.size(), mod_samples.data(), &num_written);
			mod_samples.resize(num_written);

			// Calculate amplitude ramp up
			if ((symbols.flags & start_of_burst) && si < conf.ramp_up_duration) {
				const size_t ramp_duration = conf.ramp_up_duration * mod_rate;
				const size_t hamming_window_start = si * mod_rate;
				const size_t hamming_window_len = 2 * ramp_duration;
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
	 * Generating random symbols to
	 */
	size_t trailer_length = resamp_crcf_get_delay(l_resamp); // [symbols]
	mod_samples.resize(trailer_length);
	for (size_t i = 0; i < trailer_length; i++)
		mod_samples[i] = 0.0f;

	unsigned int num_written = 0;
	resamp_crcf_execute_block(l_resamp, mod_samples.data(), mod_samples.size(), mod_samples.data(), &num_written);
	mod_samples.resize(num_written);
	co_yield mod_samples;

	reset();
}

void PSKModulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
}



Block* createPSKModulator(const Kwargs &args)
{
	return new PSKModulator();
}

static Registry registerSinkMux("PSKModulator", &createPSKModulator);
