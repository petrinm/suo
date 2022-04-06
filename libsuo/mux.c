
#include <stdlib.h>
#include "mux.h"


struct sample_sink_list {
	sample_sink_t sink;
	void* arg;
	struct sample_sink_list* next;
};

struct sample_sink_mux {
	struct sample_sink_list* sinks;
};

static void* sample_sink_mux_init(const void* confv) {
	(void)confv;
	struct sample_sink_mux* self = calloc(1, sizeof(struct sample_sink_mux));
	return self;
}

static int sample_sink_mux_sink_samples(void* arg, const sample_t *samples, size_t num_samples, suo_timestamp_t timestamp) {
	struct sample_sink_mux *self = arg;
	struct sample_sink_list* iter = self->sinks;
	while (iter != NULL) {
		iter->sink(iter->arg, samples, num_samples, timestamp);
		iter = iter->next;
	}
	return SUO_OK;
}


static int sample_sink_mux_destroy(void* arg) {
	struct sample_sink_mux *self = arg;
	if (self == NULL)
		return SUO_OK;

	struct sample_sink_list* iter = self->sinks;
	while (iter != NULL) {
		struct sample_sink_list* next = iter->next;
		iter->next = NULL;
		free(iter);
		iter = next;
	}

	self->sinks = NULL;

	free(self);
	return SUO_OK;
}

static int sample_sink_mux_set_sample_sink(void* arg, sample_sink_t callback, void* callback_arg) {
	struct sample_sink_mux *self = arg;
	if (callback == NULL || callback_arg == NULL)
		return suo_error(-3, "NULL sample source");

	// Linked list append
	struct sample_sink_list* iter = NULL;
	if (self->sinks == NULL) {
		iter = self->sinks = calloc(1, sizeof(struct sample_sink_list));
	}
	else {
		iter = self->sinks;
		while (iter->next != NULL)
			iter = iter->next;
		iter->next = calloc(1, sizeof(struct sample_sink_list));
		iter = iter->next;
	}

	iter->sink = callback;
	iter->arg = callback_arg;
	return SUO_OK;
}

const struct sample_mux_code sample_sink_mux_code = {
	.name = "sample_sink_mux",
	.init = sample_sink_mux_init,
	.destroy = sample_sink_mux_destroy,
	.set_sample_sink = sample_sink_mux_set_sample_sink,
	.sink_samples = sample_sink_mux_sink_samples,
};


/***********************************************************/

struct sample_source_list {
	sample_source_t source;
	void* arg;
	struct sample_source_list* next;
};

struct sample_source_mux {
	struct sample_source_list* sources;
	struct sample_source_list* active;
};


static void* sample_source_mux_init(const void* confv) {
	(void)confv;
	struct sample_source_mux* self = calloc(1, sizeof(struct sample_source_mux));
	return self;
}

static int sample_source_mux_source_samples(void* arg, const sample_t *samples, size_t num_samples, suo_timestamp_t timestamp) {
	int ret = 0;
	struct sample_source_mux *self = arg;
	if (self->active == NULL) {
		struct sample_source_mux *self = arg;
		struct sample_source_list* iter = self->sources;
		while (iter != NULL) {
			if ((ret = iter->source(iter->arg, samples, num_samples, timestamp)) > 0) {
				/* Start of transmission */
				self->active = iter;
				break;
			}
			iter = iter->next;
		}
	}
	else {
		if ((ret = self->active->source(self->active->arg, samples, num_samples, timestamp)) == 0) {
			/* End of transmission */
			self->active = NULL;
		}
	}

	return ret;
}


static int sample_source_mux_destroy(void* arg) {
	struct sample_source_mux *self = arg;
	if (self == NULL)
		return SUO_OK;

	struct sample_source_list* iter = self->sources;
	while (iter != NULL) {
		struct sample_source_list* next = iter->next;
		iter->next = NULL;
		free(iter);
		iter = next;
	}

	self->sources = NULL;
	self->active = NULL;
	free(self);

	return SUO_OK;
}


static int sample_source_mux_set_sample_source(void* arg, sample_source_t callback, void* callback_arg) {
	struct sample_source_mux *self = arg;
	if (callback == NULL || callback_arg == NULL)
		return suo_error(-3, "NULL sample source");

	// Linked list append
	struct sample_source_list* iter = NULL;
	if (self->sources == NULL) {
		iter = self->sources = calloc(1, sizeof(struct sample_source_list));
	}
	else {
		iter = self->sources;
		while (iter->next != NULL)
			iter = iter->next;
		iter->next = calloc(1, sizeof(struct sample_source_list));
		iter = iter->next;
	}

	iter->source = callback;
	iter->arg = callback_arg;
	return SUO_OK;
}

/*
const struct encoder_code sample_source_mux_code = {
	.name = "sample_source_mux",
	.init = sample_source_mux_init,
	.destroy = sample_source_mux_destroy,
	.set_sample_source = sample_source_mux_set_sample_source,
};
*/
