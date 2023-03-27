#include <iostream>
#include <algorithm> // clamp
//#include <format>
#include "plotter.hpp"

#include "modem/demod_fsk_mfilt.hpp"
#include "registry.hpp"


using namespace std;
using namespace suo;



FSKMatchedFilterDemodulator::Config::Config() {
	sample_rate = 1e6;
	symbol_rate = 9600;
	modindex = 0.5;
	deviation = 0;
	bits_per_symbol = 1;
	samples_per_symbol = 4;
	bt = 0.3;
	center_frequency = 100000;
	frequency_offset = 0.0f;
	pll_bandwidth0 = 0.02f; // < 0.1f;
	pll_bandwidth1 = 0.01f;
}


FSKMatchedFilterDemodulator::FSKMatchedFilterDemodulator(const Config& conf) :
	conf(conf)
{
	if (conf.sample_rate < 0)
		throw SuoError("FSKMatchedFilterDemodulator: Negative sample rate! %f", conf.sample_rate);
	if (conf.symbol_rate < 0)
		throw SuoError("FSKMatchedFilterDemodulator: Negative sample rate! %f", conf.symbol_rate);

	if (conf.modindex < 0)
		throw SuoError("FSKMatchedFilterDemodulator: Negative modindex! %f", conf.modindex);
	if (conf.deviation < 0)
		throw SuoError("FSKMatchedFilterDemodulator: Negative deviation! %f", conf.deviation);
	if (conf.modindex != 0 && conf.deviation != 0)
		throw SuoError("FSKMatchedFilterDemodulator: Both modindex and deviation defined!");

	if (conf.samples_per_symbol < 2)
		throw SuoError("FSKMatchedFilterDemodulator: samples_per_symbol < 1! %f", conf.samples_per_symbol);

	/* Configure a resampler for a fixed conf.samples_per_symbol ratio */
	float resamprate = conf.symbol_rate * conf.samples_per_symbol / conf.sample_rate;
	if (resamprate < 1)
		throw SuoError("FSKMatchedFilterDemodulator: resamprate < 1! %f", resamprate);

	double bw = 0.4 * resamprate / conf.samples_per_symbol;
	int semilen = lroundf(1.0f / bw);
	l_resamp = resamp_crcf_create(resamprate, semilen, bw, 60.0f, 16);

	// TODO: 
	resamp_crcf_get_delay(l_resamp);

	/* Calculate maximum number of output samples after feeding one sample
	 * to the resampler. This is needed to allocate a big enough array. */
	resampint = ceilf(1 / resamprate);
	sample_ns = roundf(1.0e9 / conf.sample_rate);

	/* NCO:
	 * Limit AFC range to half of symbol rate to keep it
	 * from wandering too far */
	nco_1Hz = pi2f / conf.sample_rate;
	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_pll_set_bandwidth(l_nco, nco_1Hz * conf.pll_bandwidth0 / conf.samples_per_symbol);
	update_nco();

	/* afc_speed is maximum adjustment of frequency per input sample.
	 * Convert Hz/sec into it. */
	float afc_hzsec = 0.01f * conf.symbol_rate * conf.symbol_rate;
	afc_speed = nco_1Hz * afc_hzsec / conf.sample_rate;

	constellation_size = 1 << conf.bits_per_symbol;

	/* Generate matched filters */
	const size_t filter_delay = conf.samples_per_symbol;
	cpfskmod mod = cpfskmod_create(conf.bits_per_symbol, conf.modindex,
		conf.samples_per_symbol, 2, conf.bt, LIQUID_CPFSK_GMSK);

	const size_t matched_filter_len = 3 * conf.samples_per_symbol;
	const size_t xxxx = 4 * conf.samples_per_symbol;
	Sample mod_output[xxxx];
	Sample matched_filter[xxxx];
	for (Symbol symbol_b = 0; symbol_b < constellation_size; symbol_b++) {

		for (size_t j = 0; j < xxxx; j++)
			matched_filter[j] = 0.f;

		for (Symbol symbol_a = 0; symbol_a < constellation_size; symbol_a++)
		for (Symbol symbol_c = 0; symbol_c < constellation_size; symbol_c++) {
			//cout << endl << "### " << (int)symbol_a << " " << (int)symbol_b << " " << (int)symbol_c << endl;

			cpfskmod_reset(mod);

			cpfskmod_modulate(mod, symbol_a, &mod_output[0 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_a, &mod_output[0 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_a, &mod_output[0 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_a, &mod_output[0 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_b, &mod_output[0 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_c, &mod_output[1 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_c, &mod_output[2 * conf.samples_per_symbol]);
			cpfskmod_modulate(mod, symbol_c, &mod_output[3 * conf.samples_per_symbol]);

			for (size_t j = 0; j < xxxx; j++)
				matched_filter[j] += mod_output[j];

		}

		for (size_t j = 0; j < xxxx; j++)
			matched_filter[j] *= hamming(j, xxxx) / 2 * constellation_size; // TODO: liquid_hamming

		matched_filters.push_back(firfilt_cccf_create(&matched_filter[conf.samples_per_symbol /*+ filter_delay*/], conf.samples_per_symbol));

#if 0
		std::vector<double> plot_i(xxxx);
		std::vector<double> plot_q(xxxx);
		for (size_t j = 0; j < xxxx; j++) {
			plot_i[j] = matched_filter[j].imag();
			plot_q[j] = matched_filter[j].real();
		}
		matplot::figure();
		matplot::hold(matplot::on);
		matplot::plot(plot_i);
		matplot::plot(plot_q);
		//matplot::title(format("Symbol %d", s));
	}
	matplot::show();
#else
	}
#endif

	l_eqfir = firfilt_rrrf_create((float*)(const float[5]){ -.5f, 0, 2.f, 0, -.5f }, 5);

	l_sync_window = windowcf_create(conf.samples_per_symbol);

#if 1
	unsigned int m = 5;                 // filter delay (symbols)
	float        beta = 0.5f;           // filter excess bandwidth factor
	unsigned int num_filters = 32;      // number of filters in the bank
	unsigned int num_symbols = 400;     // number of data symbols

	l_symsync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_ARKAISER, conf.samples_per_symbol, m, beta, num_filters);
	symsync_rrrf_set_lf_bw(l_symsync, 0.02f); // loop filter bandwidth
#endif
	reset();
}


FSKMatchedFilterDemodulator::~FSKMatchedFilterDemodulator()
{
	for (auto& l_fir: matched_filters)
		firfilt_cccf_destroy(l_fir);
	matched_filters.clear();

	firfilt_rrrf_destroy(l_eqfir);
	symsync_rrrf_destroy(l_symsync);
}

void FSKMatchedFilterDemodulator::reset() {
	receiver_lock = false;
	conf.frequency_offset = 0;
	update_nco();
	firfilt_rrrf_reset(l_eqfir);
	symsync_rrrf_reset(l_symsync);
}


void FSKMatchedFilterDemodulator::update_nco()
{
	float old_freq = nco_crcf_get_frequency(l_nco);

	float center_frequency = (conf.center_frequency + conf.frequency_offset);
	nco_crcf_set_frequency(l_nco, nco_1Hz * center_frequency);

	freq_min = nco_1Hz * (center_frequency - 0.5f * conf.symbol_rate);
	freq_max = nco_1Hz * (center_frequency + 0.5f * conf.symbol_rate);

	conf_dirty = false;
}


void FSKMatchedFilterDemodulator::sinkSamples(const SampleVector& samples, Timestamp timestamp)
{

	/* Allocate small buffers from stack */
	Sample samples2[resampint];

	if (conf_dirty && receiver_lock == false)
		update_nco();

	size_t plot_size = 6 * 200;
	std::vector<double> plot1; plot1.reserve(plot_size);
	std::vector<double> plot2; plot2.reserve(plot_size);
	std::vector<double> plot3; plot3.reserve(plot_size);
	std::vector<double> plot4; plot4.reserve(plot_size);
	size_t si;
	for (si = 0; si < samples.size(); si++) {
		unsigned nsamp2 = 0, si2;
		Sample s = samples[si];

		/* Downconvert and resample one input sample at a time */
		nco_crcf_step(l_nco);
		nco_crcf_mix_down(l_nco, s, &s);
		resamp_crcf_execute(l_resamp, s, samples2, &nsamp2);

		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++) {
			Sample s2 = samples2[si2];

			/* Run matched filters */
			float demod = 0;
			float total_power = 0;
			for (size_t symbol = 0; symbol < matched_filters.size(); symbol++) {

				// Calculate matched filter
				Sample mf_out;
				firfilt_cccf_push(matched_filters[symbol], s2);
				firfilt_cccf_execute(matched_filters[symbol], &mf_out);
				float power = norm(mf_out);
				total_power += power;

				// Map symbol powers to [-1, +1] range
				float mapped = 2.0f * symbol - constellation_size + 1.0f;
				demod += mapped * power * power;
				if (symbol == 0)
					plot1.push_back(power);
				else
					plot2.push_back(power);
				
			}
			
			est_power += (total_power - est_power) * 0.01f;
			demod /= total_power;

#if 0
			if (plot1.size() == plot1.capacity()) {
				cout << "PRKL " << plot1.size() << "  " << plot2.size() << endl;
				matplot::figure();
				matplot::hold(matplot::on);
				matplot::plot(plot1);
				matplot::plot(plot2);
				//matplot::show();
				//return;
			}
#endif
#if 1
			/* Equalization (Cancel gaussian filter) */
			firfilt_rrrf_push(l_eqfir, demod);
			firfilt_rrrf_execute(l_eqfir, &demod);
#endif
#if 0
			float freq = nco_crcf_get_frequency(l_nco);
			plot3.push_back(demod);
			plot4.push_back(freq);
			if (plot3.size() == plot3.capacity()) {
				matplot::figure();
				matplot::hold(matplot::on);
				matplot::plot(plot3);
				//matplot::plot(plot4);
				matplot::show();
				return;
			}
#endif


			/*
			 * Tune the AFC (Automatic Frequency Correction)
			 */
			if (1 || receiver_lock == false) {
				//nco_crcf_pll_step(l_nco, -demod);
				//nco_crcf_adjust_frequency(l_nco, -0.5 * demod);
				float freq = nco_crcf_get_frequency(l_nco);
				if (freq > freq_max)
					nco_crcf_set_frequency(l_nco, freq_max);
				if (freq < freq_min)
					nco_crcf_set_frequency(l_nco, freq_min);
			}

#if 0
			/* Feed-forward timing synchronizer
			 * --------------------------------
			 * Feed a rectified demodulated signal into a comb filter.
			 * When the output outputoutputof the comb filter peaks, take a symbol.
			 * When a frame is detected, keep timing free running
			 * for rest of the frame. */
			bool synced = false;
			float synced_sample = 0;

			//windowcf_push(samples_per_symbol, demod);
			
			const float comb_prev = ss_comb[ss_p];
			const float comb_prev2 = ss_comb[(ss_p+conf.samples_per_symbol-1) % conf.samples_per_symbol];
			ss_p = (ss_p+1) % conf.samples_per_symbol;
			float comb = ss_comb[ss_p];

			comb += (clamp(abs(demod), -1.f, 1.f) - comb) * 0.03f;

			ss_comb[ss_p] = comb;
			ss_p = ss_p;

			if (receiver_lock == false) {
				if(comb_prev > comb && comb_prev > comb_prev2) {
					synced = true;
					synced_sample = demod_prev;
					ss_ps = (ss_p+conf.samples_per_symbol-1) % conf.samples_per_symbol;
				}
			} else {
				if(ss_p == ss_ps) {
					synced = true;
					synced_sample = demod;
				}
			}


			/* Decisions and deframing
			 * ----------------------- */
			if (synced == true) {

				/* Process one output symbol from synchronizer */
				Symbol decision = (synced_sample >= 0) ? 1 : 0;
				//cout << decision << " ";

				if (0) { // SinkSymbol(decision, timestamp)) {

					/* If this was the first sync, collect store some metadata. */
					if (receiver_lock == false) {
						/*frame->setMetadata("cfo", nco_crcf_get_frequency(l_nco) / nco_1Hz);
						frame->setMetadata("rssi", 10.0f * log10f(est_power));
						frame->setMetadata("rssi", agc_crcf_get_rssi(l_agc));
						frame->setMetadata("bg_rssi", agc_crcf_get_rssi(l_bg_agc));*/
					}

					receiver_lock = true;
				}
				else {
					/* No sync, no lock */
					receiver_lock = false;
				}
			}
#else

			/* Run symbols sync */
			float synced_symbols[4];
			unsigned int output_symbols = 0;
			symsync_rrrf_execute(l_symsync, &demod, 1, synced_symbols, &output_symbols);
			//cout << "output_symbols " <<  output_symbols << endl;

			//assert(output_symbols <= 1);
			if (output_symbols == 0)
				continue;

			/* Process one output symbol from synchronizer */
			Timestamp symbol_time = 0; // now + si * sample_ns;
			//now -= conf.filter_delay * resampint; // or / resamprate

			Symbol decision = (synced_symbols[0] >= 0) ? 1 : 0;
			cout << (int)decision << " ";
			sinkSymbol.emit(decision, symbol_time);


#endif
			demod_prev = demod;
		}
		timestamp += sample_ns;
	}
}


void FSKMatchedFilterDemodulator::lockReceiver(bool locked, Timestamp now) {
	if (locked) {
		receiver_lock = true;
		//agc_crcf_lock(l_bg_agc);
		nco_crcf_pll_set_bandwidth(l_nco, nco_1Hz * conf.pll_bandwidth1 / conf.samples_per_symbol);
	}
	else {
		receiver_lock = false;
		//agc_crcf_lock(l_bg_agc);
		nco_crcf_pll_set_bandwidth(l_nco, nco_1Hz / conf.pll_bandwidth0 / conf.samples_per_symbol);
	}
}


void FSKMatchedFilterDemodulator::setFrequencyOffset(float frequency_offset) {
	cerr << "Downlink frequency offset " << frequency_offset;
	conf.frequency_offset = frequency_offset;
	conf_dirty = true;
}


Block* createFSKMatchedFilterDemodulator(const Kwargs &args)
{
	return new FSKMatchedFilterDemodulator();
}

static Registry registerFSKMatchedFilterDemodulator("FSKMatchedFilterDemodulator", &createFSKMatchedFilterDemodulator);
