/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * Functions converting HTSMSGs to/from JSON
 */

#ifndef HTSMSG_JSON_H_
#define HTSMSG_JSON_H_

#include "htsmsg.h"
#include "htsbuf.h"
struct rstr;
/**
 * htsmsg_binary_deserialize
 */

htsmsg_t *htsmsg_json_deserialize(const char *src);

void htsmsg_json_serialize(htsmsg_t *msg, htsbuf_queue_t *hq, int pretty);

__attribute__((warn_unused_result))
char *htsmsg_json_serialize_to_str(htsmsg_t *msg, int pretty);

struct rstr *htsmsg_json_serialize_to_rstr(htsmsg_t *msg, const char *prefix);

#endif /* HTSMSG_JSON_H_ */
