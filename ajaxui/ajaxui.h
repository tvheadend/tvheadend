/*
 *  tvheadend, AJAX user interface
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef AJAXUI_H_
#define AJAXUI_H_


typedef enum {
  AJAX_BOX_FILLED,
  AJAX_BOX_SIDEBOX,
  AJAX_BOX_BORDER,
} ajax_box_t;

void ajax_box_begin(tcp_queue_t *tq, ajax_box_t type,
		    const char *id, const char *style, const char *title);

void ajax_box_end(tcp_queue_t *tq, ajax_box_t type);

void ajax_js(tcp_queue_t *tq, const char *fmt, ...);


TAILQ_HEAD(ajax_menu_entry_queue, ajax_menu_entry);

#define AJAX_TAB_CHANNELS      0
#define AJAX_TAB_RECORDER      1
#define AJAX_TAB_CONFIGURATION 2
#define AJAX_TABS 3


void ajaxui_start(void);
void ajax_channels_init(void);
void ajax_config_init(void);


void ajax_menu_bar_from_array(tcp_queue_t *tq, const char *name, 
			      const char **vec, int num, int cur);

int ajax_channelgroup_tab(http_connection_t *hc);
int ajax_config_tab(http_connection_t *hc);

#endif /* AJAXUI_H_ */
