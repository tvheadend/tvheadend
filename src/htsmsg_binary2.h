/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * Functions converting HTSMSGs to/from a simple binary format
 */

#ifndef HTSMSG_BINARY2_H_
#define HTSMSG_BINARY2_H_

#include "htsmsg.h"

htsmsg_t *htsmsg_binary2_deserialize0(const void *data, size_t len,
                                      const void *buf);

int htsmsg_binary2_deserialize(htsmsg_t **msg, const void *data, size_t *len,
                               const void *buf);

int htsmsg_binary2_serialize0(htsmsg_t *msg, void **datap, size_t *lenp,
			      size_t maxlen);

int htsmsg_binary2_serialize(htsmsg_t *msg, void **datap, size_t *lenp,
			     size_t maxlen);

#endif /* HTSMSG_BINARY2_H_ */
