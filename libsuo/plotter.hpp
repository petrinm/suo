#pragma once

#include "suo.hpp"

namespace suo {


class Plotter
{
public:
	explicit Plotter(size_t size);
	void clear();
	bool push(double x);
	
	void plot();

	bool auto_plot;
	bool auto_clear;

	std::vector<double> data;
};

class ComplexPlotter
{
public:

	enum PlotType {
		Linear,
		FFT,
		Constellation
	};

	explicit ComplexPlotter(size_t size);
	explicit ComplexPlotter(const std::vector<Complex>& data);
	explicit ComplexPlotter(const Complex* data, size_t data_len);

	void clear();

	bool push(Complex x);

	void plot();
	void plot_constellation();
	void plot_fft();

	bool auto_plot;
	bool auto_clear;
	enum PlotType type;
	size_t nfft;

	size_t modulo;
	size_t offset;
	float sample_rate;
	float freq_offset;

	std::vector<Complex> iq;
};

}; // namespace
