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
	mod_max_samples = (unsigned int)ceil(samples_per_symbol);
	const float resamp_rate = samples_per_symbol / (float)mod_rate;

	/* Init liquid modem object to create complex symbols from bits */ 
	l_mod = modemcf_create(LIQUID_MODEM_PSK2);

	/*
	 * Init NCO for up mixing
	 */
	l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_set_frequency(l_nco, nco_1Hz * (conf.center_frequency + conf.frequency_offset));

	reset();
}


PSKModulator::~PSKModulator()
{
	modem_destroy(l_mod);
	nco_crcf_destroy(l_nco);
	resamp_crcf_destroy(l_resamp);
}

void PSKModulator::reset() {
	nco_crcf_destroy(l_nco);
	modemcf_reset(l_mod);
	resamp_crcf_reset(l_resamp);
}


void PSKModulator::sourceSamples(SampleVector& samples, Timestamp now)
{
	size_t max_samples = samples.capacity();


}

void PSKModulator::setFrequencyOffset(float frequency_offset) {
	conf.frequency_offset = frequency_offset;
}



Block* createPSKModulator(const Kwargs &args)
{
	return new PSKModulator();
}

static Registry registerSinkMux("PSKModulator", &createPSKModulator);
