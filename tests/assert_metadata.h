


void ASSERT_METADATA_EXISTS(const struct frame *frame, unsigned int ident, unsigned int type) {
	for (unsigned int i = 0; i < frame->metadata_len; i++) {
		const struct metadata* meta = &frame->metadata[i];
		if (meta->ident == ident) {
			//CU_ASSERT(meta->len != 0);
			CU_ASSERT(meta->type == type);
			return;
		}
	}
	CU_FAIL("No such metadata");
}

void ASSERT_METADATA_UINT(const struct frame *frame, unsigned int ident, unsigned int value) {
	for (unsigned int i = 0; i < frame->metadata_len; i++) {
		const struct metadata* meta = &frame->metadata[i];
		if (meta->ident == ident) {
			CU_ASSERT(meta->type == METATYPE_UINT);
			CU_ASSERT(meta->ui == value);
			return;
		}
	}
	CU_FAIL("No such metadata");
}

void ASSERT_METADATA_INT(const struct frame *frame, unsigned int ident, int value) {
	for (unsigned int i = 0; i < frame->metadata_len; i++) {
		const struct metadata* meta = &frame->metadata[i];
		if (meta->ident == ident) {
			CU_ASSERT(meta->type == METATYPE_INT);
			CU_ASSERT(meta->i == value);
			return;
		}
	}
	CU_FAIL("No such metadata");
}

void ASSERT_METADATA_FLOAT(const struct frame *frame, unsigned int ident, float value) {
	for (unsigned int i = 0; i < frame->metadata_len; i++) {
		const struct metadata* meta = &frame->metadata[i];
		if (meta->ident == ident) {
			CU_ASSERT(meta->type == METATYPE_FLOAT);
			CU_ASSERT(meta->fl == value);
			return;
		}
	}
	CU_FAIL("No such metadata");
}

void ASSERT_METADATA_DOUBLE(const struct frame *frame, unsigned int ident, double value) {
	for (unsigned int i = 0; i < frame->metadata_len; i++) {
		const struct metadata* meta = &frame->metadata[i];
		if (meta->ident == ident) {
			CU_ASSERT(meta->type == METATYPE_DOUBLE);
			CU_ASSERT(meta->dl == value);
			return;
		}
	}
	CU_FAIL("No such metadata");
}
