/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * Functions converting HTSMSGs to/from a simple binary format
 */

#ifndef HTSMSG_BINARY_H_
#define HTSMSG_BINARY_H_

#include "htsmsg.h"

/**
 * htsmsg_binary_deserialize
 */

htsmsg_t *htsmsg_binary_deserialize0(const void *data, size_t len,
                                     const void *buf);

int htsmsg_binary_deserialize(htsmsg_t **msg, const void *data, size_t *len,
                              const void *buf);

int htsmsg_binary_serialize0(htsmsg_t *msg, void **datap, size_t *lenp,
			     int maxlen);

int htsmsg_binary_serialize(htsmsg_t *msg, void **datap, size_t *lenp,
			    int maxlen);

#endif /* HTSMSG_BINARY_H_ */
