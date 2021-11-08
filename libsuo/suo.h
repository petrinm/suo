#ifndef LIBSUO_SUO_H
#define LIBSUO_SUO_H

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <math.h>
#include <stdbool.h>

/* -----------------
 * Common data types
 * ----------------- */

// Data type to represent I/Q samples in most calculations
typedef float complex sample_t;

typedef uint8_t symbol_t;
typedef float soft_symbol_t;

// Fixed-point I/Q samples
typedef uint8_t cu8_t[2];
typedef int16_t cs16_t[2];

// Data type to represent single bits. Contains a value 0 or 1.
typedef uint8_t bit_t;
typedef uint8_t byte_t;

/* Data type to represent soft decision bits.
 * 0 = very likely '0', 255 = very likely '1'.
 * Exact mapping to log-likelihood ratios is not specified yet. */
typedef uint8_t softbit_t;

// Data type to represent timestamps. Unit is nanoseconds.
typedef uint64_t timestamp_t;

/* All categories have these functions at the beginning of the struct,
 * so configuration and initialization code can be shared among
 * different categories by casting them to struct any_code.
 *
 * Maybe there would be a cleaner way to do this, such as having this
 * as a member of every struct. Let's think about that later. */
struct any_code {
	// Name of the module
	const char *name;

	// Initialize an instance based on a configuration struct
	void *(*init)      (const void *conf);

	/* Destroy an instance and free all memory allocated.
	 * Not always supported. */
	int   (*destroy)   (void *);

	/* Allocate a configuration struct, fill it with the default values
	 * and return a pointer to it. */
	void *(*init_conf) (void);

	// Set a configuration parameter
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);
};


#define SUO_OK        0
#define SUO_ERROR      -1
#define SUO_ERR_NOT_IMPLEMENTED  -3

/* Flag to prevent transmission of a frame if it's too late,
 * i.e. if the timestamp is already in the past */
#define SUO_FLAGS_NO_LATE 0x40000


#include "suo_interface.h"
#include "metadata.h"

struct frame_header {
	uint32_t id;
	uint32_t flags;
	timestamp_t timestamp; // Current time
};

// Frame together with metadata
struct frame {
	struct frame_header hdr;    // Frame header field
	struct metadata* metadata;  // Metadata
	unsigned int metadata_len;  //
	uint8_t* data; // Data (can be bytes, bits, symbols or soft bits)
	unsigned int data_len;
	unsigned int data_alloc_len;
};


typedef int(*tick_source_t)(void*, timestamp_t timestamp);
typedef int(*tick_sink_t)(void*, timestamp_t timestamp);

typedef int(*frame_source_t)(void*, struct frame *frame, timestamp_t timestamp);
typedef int(*frame_sink_t)(void*, const struct frame *frame, timestamp_t timestamp);


typedef int(*symbol_source_t)(void*, symbol_t *symbols, size_t max_symbols, timestamp_t timestamp);
//typedef int(*symbol_sink_t)(void*, symbol_t *symbols, size_t max_symbols, timestamp_t timestamp);
typedef int(*symbol_sink_t)(void*, symbol_t symbols, timestamp_t timestamp);


typedef int(*soft_symbol_source_t)(void*, soft_symbol_t *symbols, size_t max_symbols, timestamp_t timestamp);
//typedef int(*soft_symbol_sink_t)(void*, soft_symbol_t *symbols, size_t max_symbols, timestamp_t timestamp);
typedef int(*soft_symbol_sink_t)(void*, soft_symbol_t symbol, timestamp_t timestamp);

typedef int(*sample_source_t)(void*, sample_t *samples, size_t max_samples, timestamp_t timestamp);
typedef int(*sample_sink_t)(void*, const sample_t *samples, size_t num_samples, timestamp_t timestamp);


/* -----------------------------------------
 * Receive related interfaces and data types
 * ----------------------------------------- */
struct rx_output_code;


/* Interface to a frame decoder module */
struct decoder_code {
	const char *name;
	void *(*init)      (const void *conf);
	int   (*destroy)   (void *);
	void *(*init_conf) (void);
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);

	int   (*set_frame_sink) (void *, frame_sink_t callback, void *arg);
	//int   (*set_frame_source) (void *, const struct rx_output_code *source, void *source_arg);

	symbol_sink_t sink_symbol;
	symbol_sink_t sink_soft_symbol;

};


/* Interface to a receiver output module.
 * A receiver calls one when it has received a frame. */
struct rx_output_code {
	const char *name;
	void *(*init)      (const void *conf);
	int   (*destroy)   (void *);
	void *(*init_conf) (void);
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);

	// Set callback to a decoder which is used to decode a frame
	int   (*set_frame_source) (void *, frame_source_t callback, void *arg);

	// Called by a receiver when a frame has been received
	frame_sink_t sink_frame;

	// Called regularly with the time where reception is going
	tick_sink_t sink_tick;
};


/* Interface to a receiver module, which typically performs
 * demodulation, synchronization and deframing.
 * When a frame is received, a receiver calls a given rx_output.
 */
struct receiver_code {
	const char *name;
	void *(*init)          (const void *conf);
	int   (*destroy)       (void *);
	void *(*init_conf)     (void);
	int   (*set_conf)      (void *conf, const char *parameter, const char *value);

	// Set callback to an rx_output module which is called when a frame has been received
	int   (*set_symbol_sink) (void *, symbol_sink_t callback, void* arg);
	//int   (*set_soft_symbol_sink) (void *, const struct decoder_code *, void *rx_output_arg);

	sample_sink_t sink_samples;
};


/* ------------------------------------------
 * Transmit related interfaces and data types
 * ------------------------------------------ */

struct tx_input_code;

/* Interface to a frame encoder module */
struct encoder_code {
	const char *name;
	void *(*init)      (const void *conf);
	int   (*destroy)   (void *);
	void *(*init_conf) (void);
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);

	int   (*tick)      (void *, timestamp_t timenow);


	//int   (*set_symbol_sink) (void *, const struct decoder_code *, void *rx_output_arg);
	int   (*set_frame_source) (void *, frame_source_t callback, void *arg);

	symbol_source_t source_symbols;

	int   (*encode)  (void *, const struct frame *in, struct frame *out, size_t maxlen);

	//RET_CALLBACK(sample)  (*get_sample_source)(void *, unsigned int ident);

};



/* Interface to a transmitter input module.
 * A transmitter calls one to request a frame to be transmitted. */
struct tx_input_code {
	const char *name;
	void *(*init)      (const void *conf);
	int   (*destroy)   (void *);
	void *(*init_conf) (void);
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);

	// Set callback to a encoder which is used to encode a frame
	//int   (*set_callbacks) (void *, const struct encoder_code *, void *encoder_arg);
	int   (*set_frame_sink) (void *, const struct encoder_code * sink, void *arg);

	/* Called by a transmitter to request the next frame to be transmitted.
	 * time_dl is a "deadline": if there is a frame to transmit
	 * before time_dl, it should be returned in this call, since in the
	 * next call it may be too late. Returning a frame to transmit after
	 * time_dl is also accepted though. */
	int   (*source_frame) (void *, struct frame *frame, timestamp_t t);

	// Called regularly with the time where transmit signal generation is going
	int   (*tick)      (void *, timestamp_t timenow);
};


// Return value of transmitter execute
typedef struct {
	/* Number of the samples produced, also including the time
	 * outside of a burst. Transmitter code should usually
	 * aim to produce exactly the number of samples requested
	 * and some I/O drivers may assume this is the case,
	 * but exceptions are possible. */
	int len;
	// Index of the sample where transmit burst starts
	int begin;
	/* Index of the sample where transmit burst ends.
	 * If the transmission lasts the whole buffer,
	 * end is equal to len and begin is 0.
	 * If there's nothing to transmit, end is equal to begin. */
	int end;
} tx_return_t;



/* Interface to a transmitter module.
 * These perform modulation of a signal. */
struct transmitter_code {
	const char *name;
	void *(*init)      (const void *conf);
	int   (*destroy)   (void *);
	void *(*init_conf) (void);
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);

	int   (*set_tick) (void *, tick_source_t callback, void *arg);
	int   (*set_frame_source) (void *, frame_source_t callback, void *arg);
	int   (*set_symbol_source) (void *, symbol_source_t callback, void *arg);
	int   (*set_sample_source) (void *, sample_source_t callback, void *arg);

	/* Generate a buffer of signal to be transmitted.
	 * Timestamp points to the first sample in the buffer. */
	sample_source_t source_samples;

};


/* --------------------------------
 * Common interfaces and data types
 * -------------------------------- */

/* Interface to an I/O implementation
 * which typically controls some SDR hardware.
 * Received signal is passed to a given receiver.
 * Signal to be transmitted is asked from a given transmitter.
 */
struct signal_io_code {

	const char *name;
	void *(*init)      (const void *conf);
	int   (*destroy)   (void *);
	void *(*init_conf) (void);
	int   (*set_conf)  (void *conf, const char *parameter, const char *value);

	// Set callbacks to a receiver and a transmitter
	int   (*set_sample_sink)(void *, sample_sink_t callback, void *arg);
	int   (*set_sample_source)(void *, sample_source_t callback, void *arg);

	// The I/O "main loop"
	int   (*execute)    (void *);
};




// List of all receivers
extern const struct receiver_code *suo_receivers[];
// List of all transmitters
extern const struct transmitter_code *suo_transmitters[];
// List of all decoders
extern const struct decoder_code *suo_decoders[];
// List of all encoders
extern const struct encoder_code *suo_encoders[];
// List of all RX outputs
extern const struct rx_output_code *suo_rx_outputs[];
// List of all TX inputs
extern const struct tx_input_code *suo_tx_inputs[];
// List of all signal I/Os
extern const struct signal_io_code *suo_signal_ios[];

/* Allocate new frame object */
struct frame* suo_frame_new(unsigned int data_len);

void suo_frame_clear(struct frame* frame);

/**/
void suo_frame_copy(struct frame* dst, const struct frame* src);

/* Destroy */
void suo_frame_destroy(struct frame* frame);

/**/
int suo_error(int err_code, const char* err_msg);

#define SUO_PRINT_DATA      1
#define SUO_PRINT_METADATA  2
#define SUO_PRINT_COLOR     4
#define SUO_PRINT_COMPACT   8

/* Write samples somewhere for testing purposes.
 * Something like this could be useful for showing diagnostics
 * such as constellations in end applications as well,
 * so some more general way would be nice. */
void suo_print_samples(unsigned stream, sample_t *samples, size_t len);
void suo_print_symbols(symbol_t *symbols, size_t len);
void suo_frame_print(const struct frame* frame, unsigned int flags);

#endif
