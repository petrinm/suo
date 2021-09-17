#include "demod_fsk_mfilt.h"
#include "suo_macros.h"
#include <string.h>
#include <assert.h>
#include <stdio.h> // for debug prints only
#include <liquid/liquid.h>


#define FRAMELEN_MAX 0x900
#define OVERSAMPLING 4
//#define DEBUG3

static const float pi2f = 6.283185307179586f;

struct demod_fsk_mfilt {
	/* Configuration */
	struct fsk_demod_mfilt_conf c;

	//float resamprate;
	unsigned resampint;
	float nco_1Hz, afc_speed;

	/* Deframer state */
	uint64_t latest_bits;
	unsigned framepos, totalbits;
	bool receiver_lock;

	/* AFC state */
	float freq_min, freq_max, freq_center, freq_adj;

	/* Timing synchronizer state */
	float ss_comb[OVERSAMPLING];
	float demod_prev;
	unsigned ss_p, ss_ps;

	/* General metadata */
	float est_power; // Running estimate of the signal power

	//symbol_callback;
	//softsymbol_callback;

	/* liquid-dsp objects */
	nco_crcf l_nco;
	resamp_crcf l_resamp;
	firfilt_cccf l_fir0, l_fir1;
	firfilt_rrrf l_eqfir;

	/* Callbacks */
	struct rx_output_code output;
	void *output_arg;

	/* Buffers */
	struct frame frame;

};


/* Fixed matched filters for 4x oversampling, h=0.6 and BT=0.5 */
#define FIXED_MF_LEN 12
static const float complex fixed_mf0[FIXED_MF_LEN] = {
	-0.0422f-0.0581f*I,0.2284f+0.3138f*I,0.4524f+0.6064f*I,0.6247f+0.7153f*I,0.8126f+0.5769f*I,0.9750f+0.2220f*I,0.9750f-0.2220f*I,0.8126f-0.5769f*I,0.6247f-0.7153f*I,0.4524f-0.6064f*I,0.2284f-0.3138f*I,-0.0422f+0.0581f*I
};
static const float complex fixed_mf1[FIXED_MF_LEN] = {
	-0.0422f+0.0581f*I,0.2284f-0.3138f*I,0.4524f-0.6064f*I,0.6247f-0.7153f*I,0.8126f-0.5769f*I,0.9750f-0.2220f*I,0.9750f+0.2220f*I,0.8126f+0.5769f*I,0.6247f+0.7153f*I,0.4524f+0.6064f*I,0.2284f+0.3138f*I,-0.0422f-0.0581f*I
};


static inline float mag2f(float complex v)
{
	return crealf(v)*crealf(v) + cimagf(v)*cimagf(v);
}

static inline float clampf(float v, float limit)
{
	if(v != v) return 0;
	if(v >=  limit) return  limit;
	if(v <= -limit) return -limit;
	return v;
}


static void *demod_fsk_mfilt_init(const void *conf_v)
{
	struct fsk_demod_mfilt_conf c;

	/* Initialize state and copy configuration */
	struct demod_fsk_mfilt *self = malloc(sizeof(struct demod_fsk_mfilt));
	memset(self, 0, sizeof(struct demod_fsk_mfilt));
	c = self->c = *(const struct fsk_demod_mfilt_conf *)conf_v;

	/* Configure a resampler for a fixed oversampling ratio */
	float resamprate = c.symbolrate * OVERSAMPLING / c.samplerate;
	self->l_resamp = resamp_crcf_create(resamprate, 25, 0.4f / OVERSAMPLING, 60.0f, 32);
	/* Calculate maximum number of output samples after feeding one sample
	 * to the resampler. This is needed to allocate a big enough array. */
	self->resampint = ceilf(resamprate);

	/* NCO:
	 * Limit AFC range to half of symbol rate to keep it
	 * from wandering too far */
	self->nco_1Hz = pi2f / c.samplerate;
	self->freq_min = self->nco_1Hz * (c.center_freq - 0.5f*c.symbolrate);
	self->freq_max = self->nco_1Hz * (c.center_freq + 0.5f*c.symbolrate);
	self->freq_center = self->nco_1Hz * c.center_freq;
	self->l_nco = nco_crcf_create(LIQUID_NCO);
	/* afc_speed is maximum adjustment of frequency per input sample.
	 * Convert Hz/sec into it. */
	float afc_hzsec = 0.01f * c.symbolrate * c.symbolrate;
	self->afc_speed = self->nco_1Hz * afc_hzsec / c.samplerate;

	nco_crcf_set_frequency(self->l_nco, self->freq_center);

	/* Matched filters for 0 and 1 */
	self->l_fir0 = firfilt_cccf_create((float complex*)fixed_mf0, FIXED_MF_LEN);
	self->l_fir1 = firfilt_cccf_create((float complex*)fixed_mf1, FIXED_MF_LEN);
	self->l_eqfir = firfilt_rrrf_create((float*)
		(const float[5]){ -.5f, 0, 2.f, 0, -.5f }, 5);

	return self;
}


static int demod_fsk_mfilt_destroy(void *arg)
{
	/* TODO (low priority since memory gets freed in the end anyway) */
	(void)arg;
	return 0;
}


static int demod_fsk_mfilt_execute(void *arg, const sample_t *samples, size_t nsamp, timestamp_t timestamp)
{
	struct demod_fsk_mfilt *self = arg;
	self->output.tick(self->output_arg, timestamp);

	/* Copy some often used variables to local variables */
	float est_power = self->est_power;

	/* Allocate small buffers from stack */
	sample_t samples2[self->resampint];

	timestamp_t sample_ns = roundf(1.0e9f / self->c.samplerate);

	size_t si;
	for(si = 0; si < nsamp; si++) {
		unsigned nsamp2 = 0, si2;
		sample_t s = samples[si];

		/* Downconvert and resample one input sample at a time */
		nco_crcf_adjust_frequency(self->l_nco, self->freq_adj);
		nco_crcf_step(self->l_nco);
		nco_crcf_mix_down(self->l_nco, s, &s);
		resamp_crcf_execute(self->l_resamp, s, samples2, &nsamp2);
		assert(nsamp2 <= self->resampint);

		/* Process output from the resampler one sample at a time */
		for(si2 = 0; si2 < nsamp2; si2++) {
			sample_t s2 = samples2[si2];

			/*   Demodulation
			 * ----------------
			 * Compare the output amplitude of two filters.
			 * The output seems to have quite strong ISI, so just
			 * feed it into some ad-hoc FIR "equalizer"... */
			sample_t mf0out = 0, mf1out = 0;
			float power0, power1, demodr, demod = 0;

			firfilt_cccf_push(self->l_fir0, s2);
			firfilt_cccf_push(self->l_fir1, s2);

			firfilt_cccf_execute(self->l_fir0, &mf0out);
			firfilt_cccf_execute(self->l_fir1, &mf1out);
			power0 = mag2f(mf0out);
			power1 = mag2f(mf1out);

			est_power += (power1 + power0 - est_power) * 0.01f;
			self->est_power = est_power;

			demodr = (power1 - power0) / (power1 + power0);
			firfilt_rrrf_push(self->l_eqfir, demodr);
			firfilt_rrrf_execute(self->l_eqfir, &demod);


			/*
			 * Tune the AFC (Automatic Frequency Correction)
			 */
			if(!self->receiver_lock) {
				float adjustment = demod * self->afc_speed;

				float freq_now = nco_crcf_get_frequency(self->l_nco);
				if(freq_now < self->freq_min && adjustment < 0)
					adjustment = 0;
				if(freq_now > self->freq_max && adjustment >= 0)
					adjustment = 0;
				self->freq_adj = adjustment;
			} else {
				/* Lock AFC during frame */
				self->freq_adj = 0;
			}


			/* Feed-forward timing synchronizer
			 * --------------------------------
			 * Feed a rectified demodulated signal into a comb filter.
			 * When the output of the comb filter peaks, take a symbol.
			 * When a frame is detected, keep timing free running
			 * for rest of the frame. */
			unsigned synced = 0;
			float synced_sample = 0;

			unsigned ss_p = self->ss_p;
			const float comb_prev = self->ss_comb[ss_p];
			const float comb_prev2 = self->ss_comb[(ss_p+OVERSAMPLING-1) % OVERSAMPLING];
			ss_p = (ss_p+1) % OVERSAMPLING;
			float comb = self->ss_comb[ss_p];

			comb += (clampf(fabsf(demod), 1.0f) - comb) * 0.03f;

			self->ss_comb[ss_p] = comb;
			self->ss_p = ss_p;

			if (!self->receiver_lock) {
				if(comb_prev > comb && comb_prev > comb_prev2) {
					synced = 1;
					synced_sample = self->demod_prev;
					self->ss_ps = (ss_p+OVERSAMPLING-1) % OVERSAMPLING;
				}
			} else {
				if(ss_p == self->ss_ps) {
					synced = 1;
					synced_sample = demod;
				}
			}


#ifdef DEBUG3
			/* Debugging outputs */
			int write(int, const void*, size_t);
			write(3, &demod, sizeof(float));
			float asdf = nco_crcf_get_frequency(self->l_nco);
			write(3, &asdf, sizeof(float));
			write(3, &comb, sizeof(float));
			write(3, &synced_sample, sizeof(float));
#endif

			/* Decisions and deframing
			 * ----------------------- */
			if (synced == 1) {
				/* Process one output symbol from synchronizer */
				bit_t decision = (synced_sample >= 0) ? 1 : 0;
				//if (self->deframer->execute(self, decision, timestamp)) {
				if (0) {

					/* If this was the first sync, collect store some metadata. */
					if (self->receiver_lock == false) {
						float cfo = (nco_crcf_get_frequency(self->l_nco) - self->freq_center) / self->nco_1Hz;
						float rssi = 10.0f * log10f(self->est_power);
						SET_METADATA_F(&self->frame, METADATA_POWER, cfo);
						SET_METADATA_F(&self->frame, METADATA_RSSI, rssi);
					}

					self->receiver_lock = true;
				}
				else {
					/* No sync, no lock */
					self->receiver_lock = false;
				}
			}

			self->demod_prev = demod;
		}
		timestamp += sample_ns;
	}

	return 0;
}



static int demod_fsk_mfilt_set_callbacks(void *arg, const struct rx_output_code *output, void *output_arg)
{
	struct demod_fsk_mfilt *self = arg;
	self->output = *output;
	self->output_arg = output_arg;
	return 0;
}


const struct fsk_demod_mfilt_conf fsk_demod_mfilt_defaults = {
	.samplerate = 1e6,
	.symbolrate = 9600,
	.bits_per_symbol = 2,
	.center_freq = 100000,
};

CONFIG_BEGIN(fsk_demod_mfilt)
CONFIG_F(samplerate)
CONFIG_F(symbolrate)
CONFIG_I(bits_per_symbol)
CONFIG_F(center_freq)
CONFIG_END()


const struct receiver_code demod_fsk_mfilt_code = {
	.name = "demod_fsk_mfilt",
	.init = demod_fsk_mfilt_init,
	.destroy = demod_fsk_mfilt_destroy,
	.init_conf = init_conf, // Constructed by CONFIG-macro
	.set_conf = set_conf, // Constructed by CONFIG-macro
	.set_callbacks = demod_fsk_mfilt_set_callbacks,
	.execute = demod_fsk_mfilt_execute
};
