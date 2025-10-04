/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2010 Andreas Öman
 */

#ifndef TSFIX_H__
#define TSFIX_H__

#include "tvheadend.h"

streaming_target_t *tsfix_create(streaming_target_t *output);

void tsfix_destroy(streaming_target_t *gh);


#endif // TSFIX_H__
