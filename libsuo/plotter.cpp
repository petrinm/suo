#include "plotter.hpp"

#ifdef SUO_SUPPORT_PLOTTING

#include <liquid/liquid.h>
#include <matplot/matplot.h>


using namespace std;
using namespace suo;
using namespace matplot;


Plotter::Plotter(size_t size) :
	auto_plot(false),
	auto_clear(false)
{
	data.reserve(size);
}


void Plotter::clear() {
	data.clear();
}


bool Plotter::push(double x) {
	if (data.size() == data.capacity())
		return false;
	
	data.push_back(x);

	if (data.size() != data.capacity())
		return false;

	if (auto_plot)
		plot();
	if (auto_clear)
		clear();

	return true;
}

void Plotter::plot() {
	auto fig = figure();
	matplot::plot(data);
}



ComplexPlotter::ComplexPlotter(size_t size) :
	auto_plot(false),
	auto_clear(false),
	type(ComplexPlotter::Linear),
	nfft(1024),
	modulo(4),
	offset(0),
	sample_rate(10e3),
	freq_offset(0)
{
	iq.reserve(size);
}

ComplexPlotter::ComplexPlotter(const std::vector<Complex>& data) :
	ComplexPlotter(data.size())
{
	iq.resize(data.size());
	for (size_t j = 0; j < data.size(); j++)
		iq[j] = data[j];
}

ComplexPlotter::ComplexPlotter(const Complex* data, size_t data_len) :
	ComplexPlotter(data_len)
{
	iq.resize(data_len);
	for (size_t j = 0; j < data_len; j++)
		iq[j] = data[j];
}


void ComplexPlotter::clear() {
	iq.clear();
}

bool ComplexPlotter::push(Complex x) {

	if (iq.size() == iq.capacity())
		return false;

	iq.push_back(x);

	if (iq.size() < iq.capacity())
		return false;

	if (auto_plot) {
		if (type == ComplexPlotter::FFT)
			plot_fft();
		else if (type == ComplexPlotter::Constellation)
			plot_constellation();
		else
			plot();
	}

	if (auto_clear)
		clear();

	return true;
}

void ComplexPlotter::plot() {

	vector<double> i_data(iq.size()), q_data(iq.size());
	for (size_t i = 0; i < iq.size(); i++) {
		q_data[i] = iq[i].real();
		i_data[i] = iq[i].imag();
	}

	figure();
	matplot::plot(q_data, i_data);
}


void ComplexPlotter::plot_constellation() {

	vector<double> i_data; i_data.reserve(iq.size());
	vector<double> q_data; q_data.reserve(iq.size());
	for (size_t i = 0; i < iq.size(); i++) {
		if ((i % modulo) != offset) continue;
		q_data.push_back(iq[i].real());
		i_data.push_back(iq[i].imag());
	}

	figure();
	hold(on);

	auto p = matplot::plot(q_data, i_data);
	p->line_width(2);
	p->color({ 0, 0, 1, 0, 0.5 });
	p->color({ 0.3, 0.2, 0.2, 1 });

	auto s = scatter(q_data, i_data);
	s->color(color::red);
	//s->marker(line_spec::marker_style::cross);
	s->marker_size(4);
	//s->fill(true);
}


void ComplexPlotter::plot_fft() {

	Complex Yfft[nfft];

	fft_run(nfft, iq.data(), Yfft, LIQUID_FFT_FORWARD, 0);
	fft_shift(Yfft, nfft);

	vector<double> fft(nfft);
	for (size_t i = 0; i < nfft; i++) fft[i] = 0;

	for (size_t i = 0; i < nfft; i++)
		fft[i] = 10 * log10(abs(Yfft[i]));

	auto fig = figure();
	matplot::plot(fft);
	xlim({ 0.0, (double)nfft });
}

#endif /* PLOTTER */