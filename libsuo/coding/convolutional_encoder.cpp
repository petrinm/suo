
#include "coding/convolutional_encoder.hpp"
#include "framing/utils.hpp"

using namespace suo;
using namespace std;


void ConvolutionalConfig::validate() const {

	if (rate < 2 || rate > 4)
		throw SuoError("Invalid convolution coding rate");

	if (polys.size() != rate)
		throw SuoError("Given coding rate and generator polynomy count doesn't match.");

	if (puncturing.empty() == false) {
		if (puncturing.size() != rate)
			throw SuoError("Invalid puncturing vector count");

		size_t s = puncturing[0].size();
		for (auto& c : puncturing) {
			if (puncturing.size() == s)
				throw SuoError("Inconsistent puncturing vector size");
		}
	}

}

ConvolutionalEncoder::ConvolutionalEncoder(const ConvolutionalConfig& conf) :
	conf(conf),
	puncturing_index(0),
	shift_register(0)
	//num_states(1 << (conf.k - 1)),
{
	conf.validate();

	input.reserve(512);
	encoderOutput.resize(conf.rate);
}


void ConvolutionalEncoder::reset(uint32_t start_state)
{
	shift_register = start_state;
	puncturing_index = 0;
}

double ConvolutionalEncoder::real_rate() const
{
	if (conf.puncturing.empty()) {
		return 1.0 / (double)conf.rate;
	}
	else {
		int output_bits = 0;
		for (const auto& c : conf.puncturing) {
			for (const auto& i: c)
				output_bits += (i != 0);
		}

		return (double)conf.rate / (double)output_bits;
	}
}


void ConvolutionalEncoder::sourceSymbols(SymbolVector& symbols, Timestamp now)
{
	if (output_gen.running())
		output_gen.sourceSymbols(symbols);
	else {
		sourceUncodedSymbols.emit(input, now);
		if (input.empty() == false) {
			//if (conf.puncturing.empty())
			//	output_gen = generateSymbols();
			//else
			//	output_gen = generatePuncturedSymbols();
		}
	}
	
}


SymbolGenerator ConvolutionalEncoder::generateSymbols(SymbolGenerator& symbol_input)
{
	for (Symbol s: symbol_input)
	{
		shift_register = (shift_register << 1) | (s & 1);

		for (unsigned int j = 0; j < conf.rate; j++) {

			// Calculate generator output
			Bit g_out = bit_parity(shift_register & abs(conf.polys[j]));

			// Invert output if polynom is negative
			g_out = ((conf.polys[j] < 0) ^ g_out) != 0;

			co_yield g_out;
		}
	}

}

SymbolGenerator ConvolutionalEncoder::generatePuncturedSymbols(SymbolGenerator& symbol_input)
{
	for (Symbol s: symbol_input)
	{
		shift_register = (shift_register << 1) | (s & 1);

		const std::vector<unsigned int>& c = conf.puncturing[puncturing_index];
		if (++puncturing_index >= conf.puncturing.size())
			puncturing_index = 0;

		for (unsigned int j = 0; j < conf.rate; j++) {
			if (c[j] == 0) continue;

			// Calculate generator output
			Bit g_out = bit_parity(shift_register & abs(conf.polys[j]));

			// Invert output if polynom is negative
			g_out = ((conf.polys[j] < 0) ^ g_out) != 0;

			co_yield g_out;
		}
	}

}


namespace suo::ConvolutionCodes {

const ConvolutionalConfig AX5043 = {
	.k = 5, .rate = 2,
	.polys = {
		/* G1 */  25, // 0b11001
		/* G2 */  23, // 0b10111
	},
	.puncturing = {}
};

// ref: https://www.ti.com/lit/an/swra113a/swra113a.pdf
const ConvolutionalConfig TI_CC11xx = {
	.k = 4, .rate = 2,
	.polys = {
		/* G1 */  15, // 0b1111
		/* G2 */  11, // 0b1011
	},
	.puncturing = {}
};

const ConvolutionalConfig CCSDS_1_2_7 = {
	.k = 7, .rate = 2, 
	.polys = {
		/* G1 */   79, // 0b1001111
		/* G2 */ -109, // 0b1101101 inverted output
	},
	.puncturing = {},
};

const ConvolutionalConfig CCSDS_2_3_7 = {
	.k = 7, .rate = 2,
	.polys = {
		/* G1 */  79, // 0b1001111
		/* G2 */ -109 // 0b1101101 inverted output
	},
	.puncturing = {
		/* C1 */ { 1, 0 },
		/* C2 */ { 1, 1 },
	},
};

const ConvolutionalConfig CCSDS_3_4_7 = {
	.k = 7, .rate = 2,
	.polys = {
		/* G1 */   79, // 0b1001111
		/* G2 */ -109, // 0b1101101 inverted output
	},
	.puncturing = {
		/* C1 */ { 1, 0, 1 },
		/* C2 */ { 1, 1, 0 },
	},

};

const ConvolutionalConfig CCSDS_4_5_7 = {
	.k = 7, .rate = 2,
	.polys = {
		/* G1 */   79, // 0b1001111
		/* G2 */ -109, // 0b1101101 inverted output
	},
	.puncturing = {
		/* C1 */ { 1, 0, 1, 0, 1 },
		/* C2 */ { 1, 1, 0, 1, 0 },
	}
};

const ConvolutionalConfig CCSDS_5_6_7 = {
	.k = 7, .rate = 2,
	.polys = {
		/* G1 */   79, // 0b1001111
		/* G2 */ -109, // 0b1101101 inverted output
	},
	.puncturing = {
		/* C1 */ { 1, 0, 0, 0, 1, 0, 1 },
		/* C2 */ { 1, 1, 1, 1, 0, 1, 0 },
	}
};

const ConvolutionalConfig CCSDS_1_3_7 = {
	.k = 7, .rate = 3,
	.polys = {
		/* G1 */  104, // 0b1101101
		/* G2 */   87, // 0b1010111
		/* G3 */   79, // 0b1001111
	},
	.puncturing = {}
};


using namespace std::literals::string_view_literals;
static constexpr std::array<std::pair<std::string_view, const ConvolutionalConfig&>, 9> conv_codes {{
	{"ax5043"sv, AX5043},
	{"ti_cc11xx", TI_CC11xx},
	{"CCSDS r=1/2 k=7"sv, CCSDS_1_2_7},
	{"CCSDS r=2/3 k=7"sv, CCSDS_2_3_7},
	{"CCSDS r=3/4 k=7"sv, CCSDS_3_4_7},
	{"CCSDS r=4/5 k=7"sv, CCSDS_4_5_7},
	{"CCSDS r=5/6 k=7"sv, CCSDS_5_6_7},
	{"CCSDS r=2/3 k=7"sv, CCSDS_2_3_7},
	{"CCSDS r=1/3 k=7"sv, CCSDS_1_3_7}
}};

const ConvolutionalConfig& getConfig(const std::string_view name) {
	static const auto map = std::map<std::string_view, const ConvolutionalConfig&>{ conv_codes.begin(), conv_codes.end() };
	try {
		return map.at(name);
	}
	catch (std::out_of_range& e) {
		throw SuoError("No such convolutional code '%s'", name);
	}
}

}; // namespace ConvolutionCodes
