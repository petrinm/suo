#include "demod_gmsk.h"
#include "suo_macros.h"
#include <string.h>
#include <assert.h>
#include <stdio.h> // for debug prints only
#include <liquid/liquid.h>


/*
 * Higly influenced by:
 *   https://github.com/jgaeddert/liquid-dsp/blob/master/src/modem/src/gmskdem.c
*/

#define FRAMELEN_MAX 0x900
#define OVERSAMPLING 4


static const float pi2f = 6.283185307179586f;

struct demod_gmsk {
	/* Configuration */
	struct demod_gmsk_conf c;

	//float resamprate;
	unsigned int resampint;
	uint64_t syncmask;

	unsigned int k;                // filter samples/symbol
	unsigned int m;                // filter semi-length (symbols)

	/* Deframer state */
	enum {
		STATE_DETECTFRAME=0,        // detect frame (seek p/n sequence)
		STATE_RXPREAMBLE,           // receive p/n sequence
		STATE_RXPAYLOAD,            // receive payload data
		STATE_HELLO,
	} state;

	//uint32_t total_samples;
	uint64_t latest_bits;
	unsigned framepos, totalbits;
	bool receiving_frame;

	unsigned int mod_rate;

	float nco_1Hz;
	float center_frequency; // Currently set center frequency

	/* liquid-dsp objects */
	nco_crcf l_nco;
	resamp_crcf l_resamp;
	gmskdem l_demod;

	/* Callbacks */
	struct rx_output_code output;
	void *output_arg;

	float fi_hat;
	float complex x_prime;

	// timing recovery objects, states
	firpfb_rrrf l_mf;               // matched filter decimator
	firpfb_rrrf l_dmf;              // derivative matched filter decimator
	unsigned int npfb;              // number of filters in symsync
	float pfb_q;                    // filtered timing error
	float pfb_soft;                 // soft filterbank index
	int pfb_index;                  // hard filterbank index
	int pfb_timer;                  // filterbank output flag
	float symsync_out;              // symbol synchronizer output


	// synchronizer objects
	detector_cccf l_frame_detector; // pre-demod detector
	float tau_hat;                  // fractional timing offset estimate
	float dphi_hat;                 // carrier frequency offset estimate
	float gamma_hat;                // channel gain estimate
	windowcf l_buffer;              // pre-demod buffered samples, size: k*(pn_len+m)
	nco_crcf l_nco_coarse;          // coarse carrier frequency recovery


	unsigned int preamble_counter;
	float* preamble_pn; // preamble p/n sequence (known)
	float* preamble_rx; // preamble p/n sequence (received)

	CALLBACK(symbol_sink_t, symbol_sink);
	CALLBACK(soft_symbol_sink_t, soft_symbol_sink);

	/* Buffers */
	struct frame frame;

	unsigned int preamble_len;

};

static int execute_sample(struct demod_gmsk *self, float complex _x);
static int update_fi(struct demod_gmsk *self, float complex _x);


static void *demod_gmsk_init(const void *conf_v)
{
	/* Initialize state and copy configuration */
	struct demod_gmsk *self = calloc(1, sizeof(struct demod_gmsk));
	self->c = *(struct demod_gmsk_conf *)conf_v;

	self->k = OVERSAMPLING;
	self->m = 4;

	//self->syncmask = (1ULL << c.synclen) - 1;
	//self->framepos = c.framelen;

	/* Configure a resampler for a fixed oversampling ratio */
	float resamprate = self->c.symbol_rate * OVERSAMPLING / self->c.sample_rate;
	self->l_resamp = resamp_crcf_create(resamprate, 25, 0.4f / OVERSAMPLING, 60.0f, 32);
	/* Calculate maximum number of output samples after feeding one sample
	 * to the resampler. This is needed to allocate a big enough array. */
	self->resampint = ceilf(resamprate);


	self->center_frequency = self->c.center_frequency;
	self->nco_1Hz = pi2f / self->c.sample_rate;

	self->l_nco = nco_crcf_create(LIQUID_NCO);
	nco_crcf_set_frequency(self->l_nco, self->nco_1Hz * self->center_frequency);


	/*
	 * Frame detector
	 */
	self->preamble_len = self->c.sync_len;
	self->preamble_pn = (float*)malloc(self->preamble_len * sizeof(float));
	self->preamble_rx = (float*)malloc(self->preamble_len * sizeof(float));
	sample_t syncword_samples[self->preamble_len * OVERSAMPLING];

	gmskmod mod = gmskmod_create(OVERSAMPLING, self->m, self->c.bt);

	for (unsigned int i=0; i<self->c.sync_len + self->m; i++) {

		unsigned char bit;
		if (1 || i < self->m)
			bit = (self->c.sync_word & (self->c.sync_len - i)) != 0;

		// save p/n sequence
		if (i < self->preamble_len)
			self->preamble_pn[i] = bit ? 1.0f : -1.0f;

		// modulate/interpolate
		if (i < self->m)
			gmskmod_modulate(mod, bit, &syncword_samples[0]);
		else
			gmskmod_modulate(mod, bit, &syncword_samples[(i - self->m) * OVERSAMPLING]);
	}
	gmskmod_destroy(mod);

	// create frame detector
	float threshold = 0.5f;     // detection threshold
	float dphi_max  = 0.05f;    // maximum carrier offset allowable
	self->l_frame_detector = detector_cccf_create(syncword_samples, self->preamble_len * self->k, threshold, dphi_max);
	self->l_buffer = windowcf_create(self->k * (self->preamble_len + self->m));

	// create symbol timing recovery filters
	self->npfb = 32;   // number of filters in the bank
	self->l_mf   = firpfb_rrrf_create_rnyquist( LIQUID_FIRFILT_GMSKRX, self->npfb, self->k, self->m, self->c.bt);
	self->l_dmf  = firpfb_rrrf_create_drnyquist(LIQUID_FIRFILT_GMSKRX, self->npfb, self->k, self->m, self->c.bt);

	// create down-coverters for carrier phase tracking
	self->l_nco_coarse = nco_crcf_create(LIQUID_NCO);


	self->state = STATE_DETECTFRAME;
	//demod_gmsk_reset(self);

	return self;
}



static int demod_gmsk_reset(void *arg) {
	struct demod_gmsk *self = arg;
	self->state = STATE_DETECTFRAME;

	windowcf_reset(self->l_buffer);
	detector_cccf_reset(self->l_frame_detector);
	nco_crcf_reset(self->l_nco_coarse);

	self->preamble_counter = 0;

	self->x_prime = 0.0f;
	self->fi_hat  = 0.0f;

	firpfb_rrrf_reset(self->l_mf);
	firpfb_rrrf_reset(self->l_dmf);
	self->pfb_q = 0.0f;
	return SUO_OK;
}


static int demod_gmsk_destroy(void *arg)
{
	/* TODO (low priority since memory gets freed in the end anyway) */
	struct demod_gmsk *self = malloc(sizeof(struct demod_gmsk));

	//iirfilt_crcf_destroy(self->l_prefilter); // pre-demodulator filter
	firpfb_rrrf_destroy(self->l_mf);                // matched filter
	firpfb_rrrf_destroy(self->l_dmf);               // derivative matched filter
	nco_crcf_destroy(self->l_nco_coarse);           // coarse NCO

	// preamble
	detector_cccf_destroy(self->l_frame_detector);
	windowcf_destroy(self->l_buffer);
	free(self->preamble_pn);
	free(self->preamble_rx);

	return SUO_OK;
}



// update symbol synchronizer internal state (filtered error, index, etc.)
//  _q      :   frame synchronizer
//  _x      :   input sample
//  _y      :   output symbol
static int update_symbol_sync(struct demod_gmsk *self, float _x, float *  _y)
{
	// push sample into filterbanks
	firpfb_rrrf_push(self->l_mf,  _x);
	firpfb_rrrf_push(self->l_dmf, _x);

	//
	float mf_out  = 0.0f;    // matched-filter output
	float dmf_out = 0.0f;    // derivatived matched-filter output


	int sample_available = 0;

	// compute output if timeout
	if (self->pfb_timer <= 0) {
		sample_available = 1;

		// reset timer
		self->pfb_timer = self->k;  // k samples/symbol

		firpfb_rrrf_execute(self->l_mf,  self->pfb_index, &mf_out);
		firpfb_rrrf_execute(self->l_dmf, self->pfb_index, &dmf_out);

		// update filtered timing error
		// lo  bandwidth parameters: {0.92, 1.20}, about 100 symbols settling time
		// med bandwidth parameters: {0.98, 0.20}, about 200 symbols settling time
		// hi  bandwidth parameters: {0.99, 0.05}, about 500 symbols settling time
		self->pfb_q = 0.99f*self->pfb_q + 0.05f*crealf( conjf(mf_out)*dmf_out );

		// accumulate error into soft filterbank value
		self->pfb_soft += self->pfb_q;

		// compute actual filterbank index
		self->pfb_index = roundf(self->pfb_soft);

		// contrain index to be in [0, npfb-1]
		while (self->pfb_index < 0) {
			self->pfb_index += self->npfb;
			self->pfb_soft  += self->npfb;

			// adjust pfb output timer
			self->pfb_timer--;
		}
		while (self->pfb_index > self->npfb-1) {
			self->pfb_index -= self->npfb;
			self->pfb_soft  -= self->npfb;

			// adjust pfb output timer
			self->pfb_timer++;
		}
		//printf("  b/soft    :   %12.8f\n", self->pfb_soft);
	}

	// decrement symbol timer
	self->pfb_timer--;

	// set output and return
	*_y = mf_out / (float)(self->k);

	return sample_available;
}



// push buffered p/n sequence through synchronizer
static int pushpn(struct demod_gmsk *self)
{
	unsigned int i;

	// reset filterbanks
	firpfb_rrrf_reset(self->l_mf);
	firpfb_rrrf_reset(self->l_dmf);

	// read buffer
	float complex * rc;
	windowcf_read(self->l_buffer, &rc);

	// compute delay and filterbank index
	//  tau_hat < 0 :   delay = 2*k*m-1, index = round(   tau_hat *npfb), flag = 0
	//  tau_hat > 0 :   delay = 2*k*m-2, index = round((1-tau_hat)*npfb), flag = 0
	assert(self->tau_hat < 0.5f && self->tau_hat > -0.5f);
	unsigned int delay = 2*self->k*self->m - 1; // samples to buffer before computing output
	self->pfb_soft       = -self->tau_hat*self->npfb;
	self->pfb_index      = (int) roundf(self->pfb_soft);
	while (self->pfb_index < 0) {
		delay         -= 1;
		self->pfb_index += self->npfb;
		self->pfb_soft  += self->npfb;
	}
	self->pfb_timer = 0;

	// set coarse carrier frequency offset
	nco_crcf_set_frequency(self->l_nco_coarse, self->dphi_hat);

	unsigned int buffer_len = (self->preamble_len + self->m) * self->k;
	for (i=0; i<delay; i++) {
		float complex y;
		nco_crcf_mix_down(self->l_nco_coarse, rc[i], &y);
		nco_crcf_step(self->l_nco_coarse);

		// update instantanenous frequency estimate
		update_fi(self, y);

		// push initial samples into filterbanks
		firpfb_rrrf_push(self->l_mf,  self->fi_hat);
		firpfb_rrrf_push(self->l_dmf, self->fi_hat);
	}

	// set state (still need a few more samples before entire p/n
	// sequence has been received)
	self->state = STATE_RXPREAMBLE;

	for (i=delay; i<buffer_len; i++) {
		// run remaining samples through sample state machine
		execute_sample(self, rc[i]);
	}
	return 0;
}


// update instantaneous frequency estimate
static int update_fi(struct demod_gmsk *self, float complex _x)
{
	// compute differential phase
	self->fi_hat = cargf(conjf(self->x_prime)*_x) * self->k;

	// update internal state
	self->x_prime = _x;
	return 0;
}


/***********************/


static int execute_detectframe(struct demod_gmsk *self, float complex _x)
{
	// push sample into pre-demod p/n sequence buffer
	windowcf_push(self->l_buffer, _x);

	// push through pre-demod synchronizer
	int detected = detector_cccf_correlate(self->l_frame_detector, _x,
		&self->tau_hat,  &self->dphi_hat, &self->gamma_hat);

	// check if frame has been detected
	if (detected) {
#if 1
		printf("***** frame detected! tau-hat:%8.4f, dphi-hat:%8.4f, gamma:%8.2f dB\n",
		        self->tau_hat, self->dphi_hat, 20*log10f(self->gamma_hat));
#endif
		// push buffered samples through synchronizer
		// NOTE: state will be updated to STATE_RXPREAMBLE internally
		pushpn(self);
	}
	return 0;
}


static int execute_rxpreamble(struct demod_gmsk *self, float complex _x)
{
	if (self->preamble_counter == self->preamble_len) {
		demod_gmsk_reset(self);
		return suo_error(-999, "Preample fault");
	}
	// mix signal down
	float complex y;
	nco_crcf_mix_down(self->l_nco_coarse, _x, &y);
	nco_crcf_step(self->l_nco_coarse);

	// update instantanenous frequency estimate
	update_fi(self, y);

	// update symbol synchronizer
	float mf_out = 0.0f;
	int sample_available = update_symbol_sync(self, self->fi_hat, &mf_out);

	// compute output if timeout
	if (sample_available) {
		// save output in p/n symbols buffer
		self->preamble_rx[ self->preamble_counter ] = mf_out / (float)(self->k);

		// update counter
		self->preamble_counter++;

		if (self->preamble_counter == self->preamble_len) {
			//syncpn(self);
			self->state = STATE_RXPAYLOAD;
		}
	}
	return 0;
}


static int execute_rxpayload(struct demod_gmsk *self, float complex _x)
{
	// mix signal down
	float complex y;
	nco_crcf_mix_down(self->l_nco_coarse, _x, &y);
	nco_crcf_step(self->l_nco_coarse);

	// update instantanenous frequency estimate
	update_fi(self, y);

	// update symbol synchronizer
	float mf_out = 0.0f;
	int sample_available = update_symbol_sync(self, self->fi_hat, &mf_out);

	// compute output if timeout
	if (sample_available) {
		// demodulate
		unsigned char s = mf_out > 0.0f ? 1 : 0;

		//printf("%d ", s);
		if (self->state == STATE_RXPREAMBLE) {
			if (self->symbol_sink(self->symbol_sink_arg, s, 0) == 1) {
				self->state = STATE_HELLO;
			}
		}
		else {
			if (self->symbol_sink(self->symbol_sink_arg, s, 0) == 0) {
				demod_gmsk_reset(self);
			}
		}
	}
	return 0;
}



static int execute_sample(struct demod_gmsk *self, float complex _x)
{
	switch (self->state) {
	case STATE_DETECTFRAME: return execute_detectframe(self, _x);
	case STATE_RXPREAMBLE:  return execute_rxpreamble (self, _x);
	case STATE_RXPAYLOAD:   return execute_rxpayload  (self, _x);
	case STATE_HELLO:  return execute_rxpayload  (self, _x);
	default:;
	}
	return suo_error(-9999, "GMSK fault state");
}


static int demod_gmsk_sink_samples(void *arg, const sample_t *samples, size_t nsamp, suo_timestamp_t timestamp)
{
	struct demod_gmsk *self = arg;
	//self->output.tick(self->output_arg, timestamp);

	/* Allocate small buffers from stack */
	sample_t samples2[self->resampint];

	suo_timestamp_t sample_ns = roundf(1.0e9f / self->c.sample_rate);

	size_t si;
	for(si = 0; si < nsamp; si++) {
		unsigned nsamp2 = 0, si2;
		sample_t s = samples[si];

		/* Downconvert and resample one input sample at a time */
		//nco_crcf_adjust_frequency(self->l_nco, self->freq_adj);
		nco_crcf_step(self->l_nco);
		nco_crcf_mix_down(self->l_nco, s, &s);
		resamp_crcf_execute(self->l_resamp, s, samples2, &nsamp2);
		assert(nsamp2 <= self->resampint);

		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++)
			execute_sample(self, samples2[si2]);

		timestamp += sample_ns;
	}

	return 0;
}



static int demod_gmsk_set_symbol_sink(void *arg, symbol_sink_t callback, void *callback_arg)
{
	struct demod_gmsk *self = arg;
	self->symbol_sink = callback;
	self->symbol_sink_arg = callback_arg;
	return SUO_OK;
}



const struct demod_gmsk_conf demod_gmsk_defaults = {
	.sample_rate = 1e6,
	.symbol_rate = 9600,
	.center_frequency = 100000,
	.bt=0.3,
};


CONFIG_BEGIN(demod_gmsk)
CONFIG_F(sample_rate)
CONFIG_F(symbol_rate)
CONFIG_F(center_frequency)
CONFIG_F(bt)
CONFIG_END()


const struct receiver_code demod_gmsk_code = {
	.name = "demod_gmsk",
	.init = demod_gmsk_init,
	.destroy = demod_gmsk_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.set_symbol_sink = demod_gmsk_set_symbol_sink,
	.sink_samples = demod_gmsk_sink_samples
};
