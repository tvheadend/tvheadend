/*
 *  tvheadend, muxing of packets with builtin dvr muxer
 *  Copyright (C) 2012 John Törnblom
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#ifndef MUXER_MKV_H_
#define MUXER_MKV_H_

#include "muxer.h"

muxer_t* mkv_muxer_create (const muxer_config_t* m_cfg);

#endif /* MUXER_MKV_H_ */
