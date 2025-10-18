/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 John Törnblom
 *
 * tvheadend, simple muxer that just passes the input along
 */

#ifndef PASS_MUXER_H_
#define PASS_MUXER_H_

#include "muxer.h"

muxer_t *pass_muxer_create (const muxer_config_t *m_cfg, const muxer_hints_t *hints);

#endif
