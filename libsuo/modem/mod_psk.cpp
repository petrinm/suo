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

	/*
	 * Buffers
	 */
	symbols.reserve(0x900);
	mod_samples.resize(mod_rate);
	symbols_i = 0;


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
	state = Idle;
	symbols.clear();
	symbols_i = 0;
	mod_i = 0;
	mod_samples.clear();

	modemcf_reset(l_mod);
	nco_crcf_reset(l_nco);
	resamp_crcf_reset(l_resamp);
}


void PSKModulator::modulateSamples(Symbol symbol) {

	Sample mod_output[mod_rate];
	mod_samples.resize(mod_rate);

	// Calculate complex symbol from bits
	Complex complex_symbol;
	modemcf_modulate(l_mod, symbol, &complex_symbol);

	// Mix up the samples
	Complex carrier;
	for (unsigned int i = 0; i < mod_rate; i++) {
		// Generate carrier
		nco_crcf_step(l_nco);
		nco_crcf_cexpf(l_nco, &carrier);

		// Mix up the complex symbol
		mod_output[i] = complex_symbol * carrier;
	}

	// Interpolate to final sample rate
	unsigned int resampler_output = 0;
	resamp_crcf_execute_block(l_resamp, mod_output, mod_rate, mod_samples.data(), &resampler_output);

	for (size_t k = 0; k < resampler_output;k++)
		cout << mod_samples[k] << endl;
	mod_samples.resize(resampler_output);
	mod_i = 0;
}



void PSKModulator::sourceSamples(SampleVector& samples, Timestamp now)
{
	Sample* write_ptr = samples.data();
	size_t left = samples.capacity();
	samples.resize(samples.capacity());


	if (state == Idle) {
		/*
		 * Idle (check for now in-coming frames)
		 */

		// Source new symbols
		Timestamp time_end = now + (Timestamp)(sample_ns * samples.capacity()) - filter_delay;
		sourceSymbols.emit(symbols, time_end);

		if (symbols.empty() == false) {
			//if (symbols.flags.start_of_burst == false)
			//	cerr << "warning: start of burst not properly flagged!" << endl;

			state = Waiting;
			symbols.flags |= VectorFlags::start_of_burst;
			symbols_i = 0;
		}
	}

	if (state == Waiting) {
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

		// Update the mixer NCO on the correct frequency
		modemcf_reset(l_mod);
		nco_crcf_reset(l_nco);
		const float resamp_rate = resamp_crcf_get_rate(l_resamp);
		nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset) / resamp_rate);
		resamp_crcf_reset(l_resamp);

		state = Transmitting;

	}

	if (state == Transmitting) {
		/*
		 * Transmitting/generating samples
		 */
		

		while (1) {

			// Copy samples from modulator output to output sample buffer
			if (mod_i < mod_samples.size()) {

				size_t cpy = min(left, mod_samples.size() - mod_i);
				for (size_t i = 0; i < cpy; i++)
					*write_ptr++ = mod_samples[mod_i++];
				left -= cpy;

				// Output buffer full?
				if (mod_i != mod_samples.size()) {
					samples.resize(samples.capacity());
					break;
				}
			}

			// Fill the modulator output buffer
			modulateSamples(symbols[symbols_i]);

#if 1
			// Calculate amplitude ramp up
			if ((symbols.flags & start_of_burst) && symbols_i < conf.ramp_up_duration) {
				const size_t ramp_duration = conf.ramp_up_duration * mod_rate;
				const size_t hamming_window_start = symbols_i * mod_rate;
				const size_t hamming_window_len = 2 * ramp_duration;
				//cout << "Ramp up " << ramp_duration << " " << hamming_window_start << " " << hamming_window_len << endl;
				for (unsigned int i = 0; i < mod_samples.size(); i++)
					mod_samples[i] *= hamming(hamming_window_start + i, hamming_window_len); // TODO: liquid_hamming
			}
#endif

			symbols_i++;
			if (symbols_i > symbols.size()) {

				bool end_of_burst = (symbols.flags & end_of_burst) != 0;
				symbols.clear();

				// Try to source new symbols
				if (end_of_burst == 0) {
					Timestamp time_end = now + (Timestamp)(sample_ns * samples.capacity());
					sourceSymbols.emit(symbols, time_end);
					if (symbols.size() == 0)
						cerr << "Symbol buffer underrun!" << endl;
				}

				// End of burst
				if (symbols.size() == 0) {
					state = Trailer;
					symbols_i = 0;
					break;
				}
			}

		}

	}

	if (state == Trailer) {
		/*
		 * Generating random symbols to
		 */

		if (mod_samples.size() == 0) {
			size_t trailer_length = resamp_crcf_get_delay(l_resamp); // [symbols]

			Sample mod_output[trailer_length];
			for (size_t i = 0; i < trailer_length; i++) 
				mod_output[i] = 0.0f;

			unsigned int num_written = 0;
			resamp_crcf_execute_block(l_resamp, mod_output, trailer_length, mod_samples.data(), &num_written);
			mod_samples.resize(num_written);
			mod_i = 0;
		}

		// Copy samples from modulator output to output sample buffer
		if (mod_i < mod_samples.size()) {

			size_t cpy = min(left, mod_samples.size() - mod_i);
			for (size_t i = 0; i < cpy; i++)
				*write_ptr++ = mod_samples[mod_i++];
			left -= cpy;

			if (mod_i != mod_samples.size()) {
				samples.resize(samples.capacity());
				return;
			}
		}

		// End of burst 
		symbols.flags |= end_of_burst;
		samples.resize(samples.capacity() - left);
		reset();
	}
}

void PSKModulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
}



Block* createPSKModulator(const Kwargs &args)
{
	return new PSKModulator();
}

static Registry registerSinkMux("PSKModulator", &createPSKModulator);
