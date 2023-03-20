
#include "coding/reed_solomon.hpp"
#include <cstring>
#include <algorithm> // min

using namespace suo;
using namespace std;

/*
 * Reed Solomon encoder and decoder with Berlekamp - Massey algorithm.
 * Highly influenced by Phil Karn's (KA9Q) libfec and libcorrect.
 * libfec is lincenced used under the terms of the GNU Lesser General Public License (LGPL) 
 * and libcorrect is licenced under term of BSD-3-Clause.
 */

ReedSolomon::ReedSolomon(const ReedSolomonConfig& _cfg) :
	cfg(_cfg)
{
	/* Check parameter ranges */
	if (cfg.symbol_size == 0 || cfg.symbol_size > 8 * sizeof(DataType))
		throw SuoError("ReedSolomon: Invalid Reed Solomon symbol size");
	if (cfg.first_consecutive_root >= (1 << cfg.symbol_size))
		throw SuoError("ReedSolomon: First consecutive root i");
	if (cfg.generator_root_gap == 0 || cfg.generator_root_gap >= (1U << cfg.symbol_size))
		throw SuoError("ReedSolomon: Primitive polynom term count doesn't match with symbol size!");
	if (cfg.num_roots >= (1U << cfg.symbol_size))
		throw SuoError("ReedSolomon: Can't have more roots than symbol values!");
	if (cfg.pad >= (1 << cfg.symbol_size) - 1 - cfg.num_roots)
		throw SuoError("ReedSolomon: Too much padding");

	/* Symbol lookup table sizes */
	symbol_count = (1 << cfg.symbol_size) - 1;
	alpha_to.resize(symbol_count + 1);
	index_of.resize(symbol_count + 1);

	/* Generate Galois field lookup tables */
	index_of[0] = symbol_count; // log(zero) = -inf
	alpha_to[symbol_count] = 0; // alpha**-inf = 0
	unsigned int sr = 1;
	for (unsigned int i = 0; i < symbol_count; i++) {
		index_of[sr] = i;
		alpha_to[i] = sr;
		sr <<= 1;
		if (sr & (1 << cfg.symbol_size))
			sr ^= cfg.primitive_polynomial;
		sr &= symbol_count;
	}

	if (sr != 1) 
		throw SuoError("ReedSolomon: Field generator polynomial is not primitive!");
	

	/* Form RS code generator polynomial from its roots */
	poly.resize(cfg.num_roots + 1);

	/* Find prim-th root of 1, used in decoding */
	iprim = 1;
	while ((iprim % cfg.generator_root_gap) != 0)
		iprim += symbol_count;
	iprim = iprim / cfg.generator_root_gap;

	for (unsigned int i = 0; i < poly.size(); i++)
		poly[i] = 0;

	poly[0] = 1;
	unsigned int root = cfg.first_consecutive_root * cfg.generator_root_gap;
	for (unsigned int i = 0; i < cfg.num_roots; i++) {
		poly[i + 1] = 1;

		/* Multiply poly[] by  @**(root + x) */
		for (unsigned int j = i; j > 0; j--) {
			if (poly[j] != 0)
				poly[j] = poly[j - 1] ^ alpha_to[(index_of[poly[j]] + root) % symbol_count];
			else
				poly[j] = poly[j - 1];
		}
		
		/* poly[0] can never be zero */
		poly[0] = alpha_to[(index_of[poly[0]] + root) % symbol_count];

		root += cfg.generator_root_gap;
	}

	/* convert poly[] to index form for quicker encoding */
	for (unsigned int i = 0; i <= cfg.num_roots; i++)
		poly[i] = index_of[poly[i]];


}




void ReedSolomon::encode(std::vector<DataType>& msg) const
{
	if (msg.size() > cfg.coded_bytes)
		throw SuoError("Too long message to be coded with Reed Solomon");

	std::vector<DataType> parity(cfg.num_roots, 0);

	const unsigned int pad = cfg.coded_bytes - msg.size();
	const unsigned int m = symbol_count - cfg.num_roots - pad;
	for (unsigned int i = 0; i < m; i++) {

		uint8_t feedback = index_of[msg[i] ^ parity[0]];
		if (feedback != symbol_count) { /* feedback term is non-zero */
#ifdef UNNORMALIZED
			/* This line is unnecessary when CCSDS_Poly[cfg.num_roots] is unity, as it must
			 * always be for the polynomials constructed by init_rs()
			 */
			feedback = (symbol_count - poly[cfg.num_roots] + feedback) % symbol_count;
#endif
			for (unsigned int j = 1; j < cfg.num_roots; j++)
				parity[j] ^= alpha_to[(feedback + poly[cfg.num_roots - j]) % symbol_count];
		}

		/* Shift */
		memmove(&parity[0], &parity[1], sizeof(uint8_t) * (cfg.num_roots - 1));
		if (feedback != symbol_count)
			parity[cfg.num_roots - 1] = alpha_to[(feedback + poly[0]) % symbol_count];
		else
			parity[cfg.num_roots - 1] = 0;
	}

	msg.insert(msg.end(), parity.begin(), parity.end());
}


unsigned int ReedSolomon::decode(std::vector<DataType>& msg) const
{
	if (msg.size() <= cfg.num_roots)
		throw SuoError("Too short message");
	if (msg.size() > cfg.coded_bytes + cfg.num_roots)
		throw SuoError("Too long message");

	const unsigned int A0 = symbol_count;
	const unsigned int NN = symbol_count;
	const unsigned int pad = cfg.coded_bytes - (msg.size() - cfg.num_roots);

	DataType u, q;
	DataType t[cfg.num_roots + 1], omega[cfg.num_roots + 1];
	DataType root[cfg.num_roots], reg[cfg.num_roots + 1], loc[cfg.num_roots];

	/* Form the syndromes; i.e., evaluate msg(x) at roots of g(x) */
	DataType s[cfg.num_roots];
	for (unsigned int i = 0; i < cfg.num_roots; i++)
		s[i] = msg[0];

	for (unsigned int j = 1; j < symbol_count - pad; j++) {
		for (unsigned int i = 0; i < cfg.num_roots; i++) {
			if (s[i] == 0)
				s[i] = msg[j];
			else 
				s[i] = msg[j] ^ alpha_to[(index_of[s[i]] + (cfg.first_consecutive_root + i) * cfg.generator_root_gap) % symbol_count];
		}
	}

	/* Convert syndromes to index form, checking for non-zero condition */
	unsigned int syn_error = 0;
	for (unsigned int i = 0; i < cfg.num_roots; i++) {
		syn_error |= s[i];
		s[i] = index_of[s[i]];
	}

	if (syn_error == 0) {
		/* If syndrome is zero, msg[] is a codeword and there are no errors to correct. */
		msg.resize(msg.size() - cfg.num_roots);
		return 0;
	}

	DataType lambda[cfg.num_roots + 1]; // Err+Eras Locator poly
	lambda[0] = 1;
	memset(&lambda[1], 0, cfg.num_roots * sizeof(DataType));

	DataType b[cfg.num_roots + 1];
	for (unsigned int i = 0;i < cfg.num_roots + 1; i++)
		b[i] = index_of[lambda[i]];

	/*
	 * Begin Berlekamp-Massey algorithm to determine error+erasure
	 * locator polynomial
	 */
	unsigned int r = 0;
	unsigned int el = 0;
	while (++r <= cfg.num_roots) {	/* r is the step number */

		/* Compute discrepancy at the r-th step in poly-form */
		DataType discr_r = 0;
		for (unsigned int i = 0; i < r; i++) {
			if ((lambda[i] != 0) && (s[r - i - 1] != A0)) {
				discr_r ^= alpha_to[(index_of[lambda[i]] + s[r - i - 1]) % symbol_count];
			}
		}
		
		discr_r = index_of[discr_r];	/* Index form */
		if (discr_r == A0) {
			/* 2 lines below: B(x) <-- x*B(x) */
			memmove(&b[1], b, cfg.num_roots * sizeof(b[0]));
			b[0] = A0;
		}
		else {
			/* 7 lines below: T(x) <-- lambda(x) - discr_r*x*b(x) */
			t[0] = lambda[0];
			for (unsigned int i = 0; i < cfg.num_roots; i++) {
				if (b[i] != A0)
					t[i + 1] = lambda[i + 1] ^ alpha_to[(discr_r + b[i]) % symbol_count];
				else
					t[i + 1] = lambda[i + 1];
			}
			if (2 * el <= r - 1) {
				el = r - el;
				/* 2 lines below: B(x) <-- inv(discr_r) *  lambda(x) */
				for (unsigned int i = 0; i <= cfg.num_roots; i++)
					b[i] = (lambda[i] == 0) ? A0 : (index_of[lambda[i]] - discr_r + symbol_count) % symbol_count;
			}
			else {
				/* 2 lines below: B(x) <-- x*B(x) */
				memmove(&b[1], b, cfg.num_roots * sizeof(b[0]));
				b[0] = A0;
			}
			memcpy(lambda, t, (cfg.num_roots + 1) * sizeof(t[0]));
		}
	}

	/* Convert lambda to index form and compute deg(lambda(x)) */
	unsigned int deg_lambda = 0;
	for (unsigned int i = 0; i < cfg.num_roots + 1; i++) {
		lambda[i] = index_of[lambda[i]];
		if (lambda[i] != A0)
			deg_lambda = i;
	}

	/* Find roots of the error+erasure locator polynomial by Chien search */
	memcpy(&reg[1], &lambda[1], cfg.num_roots * sizeof(reg[0]));
	unsigned int count = 0; /* Number of roots of lambda(x) */
	for (unsigned int i = 1, k = iprim - 1; i <= symbol_count; i++, k = (k + iprim) % symbol_count) {
		q = 1; /* lambda[0] is always 0 */
		for (unsigned int j = deg_lambda; j > 0; j--) {
			if (reg[j] != A0) {
				reg[j] = (reg[j] + j) % symbol_count;
				q ^= alpha_to[reg[j]];
			}
		}
		if (q != 0)
			continue; /* Not a root */

		/* store root (index-form) and error location number */
		root[count] = i;
		loc[count] = k;

		/* If we've already found max possible roots, abort the search to save time */
		if (++count == deg_lambda)
			break;
	}

	if (deg_lambda != count) {
		/*
		 * deg(lambda) unequal to number of roots => uncorrectable
		 * error detected
		 */
		throw ReedSolomonUncorrectable("Uncorrectable error detected");
	}

	/*
	 * Compute err+eras evaluator poly omega(x) = s(x)*lambda(x) (modulo
	 * x**cfg.num_roots). in index form. Also find deg(omega).
	 */
	unsigned int deg_omega = deg_lambda - 1;
	for (unsigned int i = 0; i <= deg_omega; i++) {
		unsigned int tmp = 0;
		for (int j = i; j >= 0; j--) {
			if ((s[i - j] != A0) && (lambda[j] != A0))
				tmp ^= alpha_to[(s[i - j] + lambda[j]) % symbol_count];
		}
		omega[i] = index_of[tmp];
	}

	/*
	 * Compute error values in poly-form. num1 = omega(inv(X(l))), num2 =
	 * inv(X(l))**(cfg.first_consecutive_root-1) and den = lambda_pr(inv(X(l))) all in poly-form
	 */
	for (int j = count - 1; j >= 0; j--) {
		unsigned int num1 = 0;
		for (int i = deg_omega; i >= 0; i--) {
			if (omega[i] != A0)
				num1 ^= alpha_to[(omega[i] + i * root[j]) % symbol_count];
		}
		
		unsigned int num2 = alpha_to[(root[j] * (cfg.first_consecutive_root - 1) + symbol_count) % symbol_count];
		unsigned int den = 0;

		/* lambda[i+1] for i even is the formal derivative lambda_pr of lambda[i] */
		for (int i = min(deg_lambda, cfg.num_roots - 1) & ~1; i >= 0; i -= 2) {
			if (lambda[i + 1] != A0)
				den ^= alpha_to[(lambda[i + 1] + i * root[j]) % symbol_count];
		}

		/* Apply error to data */
		if (num1 != 0 && loc[j] >= pad) {
			msg[loc[j] - pad] ^= alpha_to[(index_of[num1] + index_of[num2] + symbol_count - index_of[den]) % symbol_count];
		}
	}

	// Truncate the message to remove roots
	msg.resize(msg.size() - cfg.num_roots);

	return count;
}


unsigned int ReedSolomon::decode(std::vector<DataType>& msg, std::vector<unsigned int>& erasures) const {
	throw SuoError("Not implemented!");
}


void ReedSolomon::deinterleave(ByteVector& data, ByteVector& output, uint8_t pos, uint8_t i)
{
	for (int ii = 0; ii < 255; ii++)
		output[ii] = data[ii * i + pos];
}

void ReedSolomon::interleave(ByteVector& data, ByteVector& output, uint8_t pos, uint8_t i)
{
	//if (data.size() % 255)
	for (int ii = 0; ii < 255; ii++)
		output[ii * i + pos] = data[ii];
}


unsigned int suo::count_bit_errors(const ByteVector& a, const ByteVector& b)
{
	unsigned int bit_errors = 0;
	size_t data_len = min(a.size(), b.size());
	for (unsigned int i = 0; i < data_len; i++)
		bit_errors += __builtin_popcountll(a[i] ^ b[i]);
	return bit_errors;
}

namespace suo::RSCodes {

const ReedSolomonConfig CCSDS_RS_255_223 = {
	.symbol_size = 8,
	.primitive_polynomial = 0x187,  // x^8 + x^7 + x^2 + x + 1
	.first_consecutive_root = 112,
	.generator_root_gap = 11,
	.coded_bytes = 223,
	.num_roots = 32,
	.pad = 0
};

const ReedSolomonConfig CCSDS_RS_255_239 = {
	.symbol_size = 8,
	.primitive_polynomial = 0x187,  // x^8 + x^7 + x^2 + x + 1
	.first_consecutive_root = 120,
	.generator_root_gap = 11,
	.coded_bytes = 239,
	.num_roots = 16,
	.pad = 0
};


using namespace std::literals::string_view_literals;
static constexpr std::array<std::pair<std::string_view, const ReedSolomonConfig&>, 2> rs_codes{ {
	{"CCSDS RS(255,223)"sv, CCSDS_RS_255_223},
	{"CCSDS RS(255,239)"sv, CCSDS_RS_255_239},
} };

const ReedSolomonConfig& getConfig(const std::string_view name) {
	static const auto map = std::map<std::string_view, const ReedSolomonConfig&>{ rs_codes.begin(), rs_codes.end() };
	try {
		return map.at(name);
	}
	catch (std::out_of_range& e) {
		throw SuoError("No such reed solomon code '%s'", name);
	}
}

}; // namespace suo::RSCodes
