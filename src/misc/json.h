#pragma once
#include <stdint.h>

typedef struct json_deserializer {
  void *(*jd_create_map)(void *jd_opaque);
  void *(*jd_create_list)(void *jd_opaque);

  void (*jd_destroy_obj)(void *jd_opaque, void *obj);

  void (*jd_add_obj)(void *jd_opaque, void *parent,
		     const char *name, void *child);

  // str must be free'd by callee
  void (*jd_add_string)(void *jd_opaque, void *parent,
			const char *name, char *str);

  void (*jd_add_s64)(void *jd_opaque, void *parent,
		      const char *name, int64_t v);

  void (*jd_add_double)(void *jd_opaque, void *parent,
			const char *name, double d);

  void (*jd_add_bool)(void *jd_opaque, void *parent,
		      const char *name, int v);

  void (*jd_add_null)(void *jd_opaque, void *parent,
		      const char *name);

} json_deserializer_t;

void *json_deserialize(const char *src, const json_deserializer_t *jd,
		       void *opaque, char *errbuf, size_t errlen);
