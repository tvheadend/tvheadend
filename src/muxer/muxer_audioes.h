/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Dave Chapman
 *
 * "muxer" to write raw audio streams
 */

#ifndef AUDIOES_MUXER_H_
#define AUDIOES_MUXER_H_

#include "muxer.h"

muxer_t *audioes_muxer_create (const muxer_config_t *m_cfg, const muxer_hints_t *hints);

#endif
