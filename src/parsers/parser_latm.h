/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * Packet parsing functions
 */

#ifndef PARSER_LATM_H_
#define PARSER_LATM_H_

th_pkt_t *parse_latm_audio_mux_element(parser_t *t,
				       parser_es_t *st,
				       const uint8_t *data, int len);

#endif /* PARSER_LATM_H_ */
