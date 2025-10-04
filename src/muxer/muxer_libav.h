/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 John Törnblom
 *
 * tvheadend, muxing of packets with libavformat
 */

#ifndef LAV_MUXER_H_
#define LAV_MUXER_H_

#include "muxer.h"

muxer_t *lav_muxer_create (const muxer_config_t *m_cfg, const muxer_hints_t *hints);

#endif
