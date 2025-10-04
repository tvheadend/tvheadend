/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 John Törnblom
 *
 * tvheadend, muxing of packets with builtin dvr muxer
 */

#ifndef MUXER_MKV_H_
#define MUXER_MKV_H_

#include "muxer.h"

muxer_t *mkv_muxer_create (const muxer_config_t *m_cfg, const muxer_hints_t *hints);

#endif /* MUXER_MKV_H_ */
