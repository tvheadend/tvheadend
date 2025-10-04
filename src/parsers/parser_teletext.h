/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * Teletext parsing functions
 */

#ifndef TELETEXT_H
#define TELETEXT_H

#define PID_TELETEXT_BASE 0x2000

void teletext_input(parser_t *t, parser_es_t *st, const uint8_t *data, int len);

#endif /* TELETEXT_H */
