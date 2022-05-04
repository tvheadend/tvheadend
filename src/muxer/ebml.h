#ifndef EBML_H__
#define EBML_H__

#include "htsbuf.h"

void ebml_append_id(htsbuf_queue_t *q, uint32_t id);

void ebml_append_size(htsbuf_queue_t *q, uint32_t size);

void ebml_append_xiph_size(htsbuf_queue_t *q, int size);

void ebml_append_bin(htsbuf_queue_t *q, unsigned id,
		    const void *data, size_t len);

void ebml_append_string(htsbuf_queue_t *q, unsigned id, const char *str);

void ebml_append_uint(htsbuf_queue_t *q, unsigned id, int64_t ui);

void ebml_append_float(htsbuf_queue_t *q, unsigned id, float f);

void ebml_append_master(htsbuf_queue_t *q, uint32_t id, htsbuf_queue_t *p);

void ebml_append_void(htsbuf_queue_t *q);

void ebml_append_pad(htsbuf_queue_t *q, size_t pad);

void ebml_append_idid(htsbuf_queue_t *q, uint32_t id0, uint32_t id);

#endif // EBML_H__
