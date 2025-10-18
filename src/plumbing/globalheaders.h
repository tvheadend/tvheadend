/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2010 Andreas Öman
 */

#ifndef GLOBALHEADERS_H__
#define GLOBALHEADERS_H__

#include "tvheadend.h"

streaming_target_t *globalheaders_create(streaming_target_t *output);

void globalheaders_destroy(streaming_target_t *gh);


#endif // GLOBALHEADERS_H__
