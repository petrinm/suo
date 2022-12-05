#include "mod_fsk.hpp"
#include "registry.hpp"

using namespace suo;
using namespace std;


FSKModulator::Config::Config() {
	sample_rate = 1e6;
	symbol_rate = 9600;
	complexity = 2;
	center_frequency = 100000;
	frequency_offset = 0;
	modindex = 1.f;
	deviation = 0.0f;
	bt = 0.5;
	ramp_up_duration = 0;
	ramp_down_duration = 0;
}


FSKModulator::FSKModulator(const Config& _conf) :
	conf(_conf)
{
	sample_ns = round(1.0e9f / conf.sample_rate);
	nco_1Hz = pi2f / conf.sample_rate;

	const float samples_per_symbol = conf.sample_rate / conf.symbol_rate;
	mod_rate = (unsigned int)samples_per_symbol + 1;
	const float resamp_rate = samples_per_symbol / (float)mod_rate;

	symbols.resize(8 * 256);
	symbols.clear();

	if (conf.modindex < 0)
		throw SuoError("FSKModulator: Negative modindex! %f", conf.modindex);
	if (conf.deviation < 0)
		throw SuoError("FSKModulator: Negative deviation! %f", conf.deviation);
	if (conf.modindex != 0 && conf.deviation != 0)
		throw SuoError("FSKModulator: Both modindex and deviation defined!");

	if (conf.deviation != 0)
		conf.modindex = conf.deviation / conf.sample_rate; // TODO: Generalize for other than 2FSK
	else if (conf.modindex == 0)
		throw SuoError("FSKModulator: Neither mod_index or deviation given!");

	center_frequency = pi2f * conf.center_frequency / conf.sample_rate;

	/*
	 * Init NCO for FSK generating
	 */
	unsigned int cpfsk_filter_delay = 2;
	l_mod = cpfskmod_create(conf.complexity, conf.modindex, mod_rate, cpfsk_filter_delay, conf.bt, LIQUID_CPFSK_SQUARE);

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
	trailer_length = cpfsk_filter_delay + conf.ramp_down_duration;
	trailer_length += (int)ceil((float)resamp_crcf_get_delay(l_resamp) / mod_rate);

	// Calculate total delay of the start of the transmission due to gaussian filter and resampling 
	filter_delay = ceil((cpfsk_filter_delay * mod_rate + resamp_crcf_get_delay(l_resamp)) * sample_ns);


	reset();
}


FSKModulator::~FSKModulator()
{
	nco_crcf_destroy(l_nco);
	resamp_crcf_destroy(l_resamp);
	cpfskmod_destroy(l_mod);
}


void FSKModulator::reset() {
	state = Idle;
	symbols.clear();
	symbols_i = 0;
	mod_i = 0;
	mod_samples.clear();

	cpfskmod_reset(l_mod);
	nco_crcf_reset(l_nco);
	resamp_crcf_reset(l_resamp);
}


void FSKModulator::modulateSamples(Symbol symbol) {

	Sample mod_output[mod_rate];
	mod_samples.resize(mod_rate);

	// Generate samples from the symbol
	cpfskmod_modulate(l_mod, symbol, mod_output);

	// Interpolate to final sample rate
	unsigned int resampler_output = 0;
	resamp_crcf_execute_block(l_resamp, mod_output, mod_rate, mod_samples.data(), &resampler_output);

	// Mix up the samples
	nco_crcf_mix_block_up(l_nco, mod_samples.data(), mod_samples.data(), resampler_output);

	mod_samples.resize(resampler_output);
	mod_i = 0;
}


void FSKModulator::sourceSamples(SampleVector& samples, Timestamp now)
{
	Sample* write_ptr = samples.data();
	size_t left = samples.capacity();
	samples.resize(samples.capacity());

	if (state == Idle) {
		/*
		 * Idle (check for now incoming frames)
		 */
		const Timestamp time_end = now + (Timestamp)(sample_ns * samples.capacity());
		sourceSymbols.emit(symbols, time_end);

		if (symbols.empty())
			return;


		symbols.flags |= VectorFlags::start_of_burst;
		state = Waiting;
		symbols_i = 0;
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
				}
				else
					cerr << "Warning: TX frame late by " << late << "ns" << endl;
			}

			// If start time is in future, don't start sample generation yet
			const Timestamp time_end = now + (Timestamp)(sample_ns * samples.capacity());
			if (symbols.timestamp > time_end)
				return;
		}

		nco_crcf_reset(l_nco);
		nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset));
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
				
				if (left == 0) {
					cout << "FULL" << endl;
					samples.resize(samples.capacity());
					break;
				}
			}

			// Fill the modulator output buffer
			modulateSamples(symbols[symbols_i]);

			// Calculate amplitude ramp up
			if ((symbols.flags & start_of_burst) && symbols_i < conf.ramp_up_duration) {
				const size_t ramp_duration = conf.ramp_up_duration * mod_rate;
				const size_t hamming_window_start = symbols_i * mod_rate;
				const size_t hamming_window_len = 2 * ramp_duration;
				//cout << "Ramp up " << ramp_duration << " " << hamming_window_start << " " << hamming_window_len << endl;
				for (unsigned int i = 0; i < mod_samples.size(); i++)
					mod_samples[i] *= hamming(hamming_window_start + i, hamming_window_len); // TODO: liquid_hamming
			}

			symbols_i++;
			if (symbols_i > symbols.size()) {

				bool end_of_burst = (symbols.flags & end_of_burst) != 0;
				symbols.clear();

				// Try to source new symbols
				if (end_of_burst) {
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
		 * Transmitting/generating samples
		 */

		while (1) {

			// Copy samples from modulator output to output sample buffer
			if (mod_i < mod_samples.size()) {

				size_t cpy = min(left, mod_samples.size() - mod_i);
				for (size_t i = 0; i < cpy; i++)
					*write_ptr++ = mod_samples[mod_i++];
				left -= cpy;

				if (mod_i != mod_samples.size()) {
					samples.resize(samples.capacity());
					break;
				}
			}

			// Feed zeros to the modulator to "flush" the filters
			modulateSamples(0);

			// Calculate amplitude ramp down
			if (conf.ramp_down_duration > 0) {
				int end = symbols_i - trailer_length + conf.ramp_down_duration - 1;
				if (end >= 0) {
					const size_t ramp_duration = conf.ramp_down_duration * mod_rate;
					const size_t hamming_window_start = (conf.ramp_down_duration + end) * mod_rate;
					const size_t hamming_window_len = 2 * ramp_duration;
					//cout << "Ramp down " << ramp_duration << " " << hamming_window_start << " " << hamming_window_len << " " << mod_rate << endl;
					for (unsigned int i = 0; i < mod_samples.size(); i++)
						mod_samples[i] *= hamming(hamming_window_start + i, hamming_window_len); // TODO: liquid_hamming
				}
			}

			symbols_i++;
			if (symbols_i > trailer_length) {
				// Flag end of burst
				samples.flags |= end_of_burst;
				samples.resize(samples.capacity() - left);
				reset();
				break;
			}
		}

	}

}


void FSKModulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
}


Block * createFSKModulator(const Kwargs& args)
{
	return new FSKModulator();
}

static Registry registerFSKModulator("FSKModulator", &createFSKModulator);

