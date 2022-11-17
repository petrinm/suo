#include <iostream>
#include <vector>
#include "suo.hpp"

#include <liquid/liquid.h>


#include <matplot/matplot.h>

using namespace std;
using namespace suo;
using namespace matplot;

#if 0
int main() {

	unsigned int samples_per_symbol = 32;
	unsigned int complexity = 2;
	
	float modindex = 0.5;
	float bt = 0.5;


	/* Generate matched filters */
	cpfskmod mod = cpfskmod_create(complexity, modindex, samples_per_symbol, 2, 1, LIQUID_CPFSK_GMSK);
	//cpfskmod mod = cpfskmod_create(complexity, modindex, samples_per_symbol, 2, 1, LIQUID_CPFSK_SQUARE);

	const size_t matched_filter_len = 3 * samples_per_symbol;
	const size_t xxxx = 4 * samples_per_symbol;
	Sample mod_output[xxxx];

	// TODO: Works on for 2FSK
	{
		cpfskmod_reset(mod);
		unsigned int s = 0;
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[1 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[2 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[3 * samples_per_symbol]);


		size_t plot_size = 4 * 200;
		vector<double> plot1; plot1.reserve(plot_size);
		vector<double> plot2; plot2.reserve(plot_size);
		vector<double> plot3; plot3.reserve(plot_size);

		for (size_t i = 0; i < 3 * samples_per_symbol; i++) {
			plot1.push_back(mod_output[i].real());
			plot2.push_back(mod_output[i].imag());
		}

		figure();
		hold(on);
		plot(plot1);
		plot(plot2);
	}
	{
		cpfskmod_reset(mod);
		unsigned int s = 1;
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[0 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[1 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[2 * samples_per_symbol]);
		cpfskmod_modulate(mod, s, &mod_output[3 * samples_per_symbol]);


		size_t plot_size = 4 * 200;
		vector<double> plot1; plot1.reserve(plot_size);
		vector<double> plot2; plot2.reserve(plot_size);
		vector<double> plot3; plot3.reserve(plot_size);

		for (size_t i = 0; i < 3 * samples_per_symbol; i++) {
			plot1.push_back(mod_output[i].real());
			plot2.push_back(mod_output[i].imag());
		}

		figure();
		hold(on);
		plot(plot1);
		plot(plot2);
	}
	show();

	return 0;
}

#elif 0


int reset();
int modulate(unsigned int    _s, Complex* _y);
int cpfskmod_firdes(unsigned int _k, unsigned int _m, float _beta, int _type, float* _ht, unsigned int _ht_len);



unsigned int M;

unsigned int bps = 2;
float        h = 0.5;
unsigned int k = 16;
unsigned int m = 2;
float        beeta = 0.3;
int          type = LIQUID_CPFSK_GMSK;
unsigned int symbol_delay;

float* ht;
size_t ht_len;
firinterp_rrrf  interp;

float* phase_interp;
iirfilt_rrrf integrator;

vector<double> plot_a;
vector<double> plot_b;
vector<double> plot_c;

int init() {

	// derived values
	M = 1 << bps; // constellation size

	// create object depending upon input type
	float b[2] = { 0.5f,  0.5f }; // integrator feed-forward coefficients
	float a[2] = { 1.0f, -1.0f }; // integrator feed-back coefficients
	ht_len = 0;
	ht = NULL;
	unsigned int i;

	switch (type) {
	case LIQUID_CPFSK_SQUARE:
		ht_len = k;
		symbol_delay = 1;
		// modify integrator
		b[0] = 0.0f;
		b[1] = 1.0f;
		break;
	case LIQUID_CPFSK_RCOS_FULL:
		ht_len = k;
		symbol_delay = 1;
		break;
	case LIQUID_CPFSK_RCOS_PARTIAL:
		// TODO: adjust response based on 'm'
		ht_len = 3 * k;
		symbol_delay = 2;
		break;
	case LIQUID_CPFSK_GMSK:
		symbol_delay = m + 1;
		ht_len = 2 * (k) * (m)+(k)+1;
		break;
	}

	// create pulse-shaping filter and scale by modulation index
	ht = (float*)malloc(ht_len * sizeof(float));
	cpfskmod_firdes(k, m, beeta, type, ht, ht_len);
	for (i = 0; i < ht_len; i++)
		ht[i] *= M_PI * h;
	interp = firinterp_rrrf_create(k, ht, ht_len);

	// create phase integrator
	phase_interp = (float*)malloc(k * sizeof(float));
	integrator = iirfilt_rrrf_create(b, 2, a, 2);

	// reset modem object
	reset();

	return LIQUID_OK;
}

#define liquid_cexpjf(THETA) (cosf(THETA) + _Complex_I*sinf(THETA))

int reset()
{
	// reset interpolator
	firinterp_rrrf_reset(interp);

	// reset phase integrator
	iirfilt_rrrf_reset(integrator);
	return LIQUID_OK;
}


int modulate(unsigned int    _s, Complex* _y)
{
	// run interpolator
	float v = 2.0f * _s - (float)(M) + 1.0f;
	firinterp_rrrf_execute(interp, v, phase_interp);

	// integrate phase state
	unsigned int i;
	float theta;
	for (i = 0; i < k; i++) {
		// push phase through integrator
		iirfilt_rrrf_execute(integrator, phase_interp[i], &theta);
		plot_a.push_back(theta);
		plot_b.push_back(v);
		// compute output
		_y[i] = liquid_cexpjf(theta);
	}
	return LIQUID_OK;
}



// design transmit filter
int cpfskmod_firdes(unsigned int _k,
	unsigned int _m,
	float        _beta,
	int          _type,
	float* _ht,
	unsigned int _ht_len)
{
	unsigned int i;
	// create filter based on specified type
	switch (_type) {
	case LIQUID_CPFSK_SQUARE:
		// square pulse
		if (_ht_len != _k)
			return LIQUID_EICONFIG;
		for (i = 0; i < _ht_len; i++)
			_ht[i] = 1.0f;
		break;
	case LIQUID_CPFSK_RCOS_FULL:
		// full-response raised-cosine pulse
		if (_ht_len != _k)
			return LIQUID_EICONFIG;
		for (i = 0; i < _ht_len; i++)
			_ht[i] = 1.0f - cosf(2.0f * M_PI * i / (float)_ht_len);
		break;
	case LIQUID_CPFSK_RCOS_PARTIAL:
		// full-response raised-cosine pulse
		if (_ht_len != 3 * _k)
			return LIQUID_EICONFIG;
		// initialize with zeros
		for (i = 0; i < _ht_len; i++)
			_ht[i] = 0.0f;
		// adding raised-cosine pulse with half-symbol delay
		for (i = 0; i < 2 * _k; i++)
			_ht[i + _k / 2] = 1.0f - cosf(2.0f * M_PI * i / (float)(2 * _k));
		break;
	case LIQUID_CPFSK_GMSK:
		// Gauss minimum-shift keying pulse
		if (_ht_len != 2 * _k * _m + _k + 1)
			return LIQUID_EICONFIG;
		// initialize with zeros
		for (i = 0; i < _ht_len; i++)
			_ht[i] = 0.0f;
		// adding Gauss pulse with half-symbol delay
		liquid_firdes_gmsktx(_k, _m, _beta, 0.0f, &_ht[_k / 2]);
		break;
	default:
		return LIQUID_EINT;
	}

	// normalize pulse area to unity
	float ht_sum = 0.0f;
	for (i = 0; i < _ht_len; i++)
		ht_sum += _ht[i];
	for (i = 0; i < _ht_len; i++)
		_ht[i] *= 1.0f / ht_sum;

	return LIQUID_OK;
}


int main() {


	/* Generate matched filters */

	init();

	const size_t matched_filter_len = 3 * k;
	const size_t xxxx = 4 * k;
	Sample mod_output[xxxx];

	// TODO: Works on for 2FSK
	{
		reset();
		unsigned int s = 0;
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[1 * k]);
		modulate(s, &mod_output[2 * k]);
		modulate(s, &mod_output[3 * k]);


		size_t plot_size = 4 * 200;
		vector<double> plot1; plot1.reserve(plot_size);
		vector<double> plot2; plot2.reserve(plot_size);
		vector<double> plot3; plot3.reserve(plot_size);

		for (size_t i = 0; i < 3 * k; i++) {
			plot1.push_back(mod_output[i].real());
			plot2.push_back(mod_output[i].imag());
		}

		figure();
		hold(on);
		plot(plot1);
		plot(plot2);
	}
	{
		reset();
		unsigned int s = 1;
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[0 * k]);
		modulate(s, &mod_output[1 * k]);
		modulate(s, &mod_output[2 * k]);
		modulate(s, &mod_output[3 * k]);


		size_t plot_size = 4 * 200;
		vector<double> plot1; plot1.reserve(plot_size);
		vector<double> plot2; plot2.reserve(plot_size);
		vector<double> plot3; plot3.reserve(plot_size);

		for (size_t i = 0; i < 3 * k; i++) {
			plot1.push_back(mod_output[i].real());
			plot2.push_back(mod_output[i].imag());
		}

		figure();
		hold(on);
		plot(plot1);
		plot(plot2);
	}


	figure();
	hold(on);
	plot(plot_a);

	figure();
	hold(on);
	plot(plot_b);
	show();

	return 0;
}


#else 


int main() {

	unsigned int k = 16;
	unsigned int m = 2;

	Complex out[k];
	gmskmod mod = gmskmod_create(k, m, 0.3);

	{
		gmskmod_reset(mod);
		vector<double> plot_a;
		vector<double> plot_b;
		for (int o = 0; o < 10; o++ ) {
			gmskmod_modulate(mod, 0, out);
			for (size_t i = 0; i < k; i++) {
				plot_a.push_back(out[i].real());
				plot_b.push_back(out[i].imag());
			}
		}
		
		figure();
		hold(on);
		plot(plot_a);
		plot(plot_b);
	}

	{
		gmskmod_reset(mod);
		vector<double> plot_a;
		vector<double> plot_b;
		for (int o = 0; o < 10; o++) {
			gmskmod_modulate(mod, 1, out);
			for (size_t i = 0; i < k; i++) {
				plot_a.push_back(out[i].real());
				plot_b.push_back(out[i].imag());
			}
		}

		figure();
		hold(on);
		plot(plot_a);
		plot(plot_b);
	}

	show();
}
#endif