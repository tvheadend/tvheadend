/*
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "tvheadend.h"
#include "libaesdec.h"


void libaesdec_init(void) {
	tvhlog(LOG_INFO, "CSA", "Using AES descrambling");
}

int get_internal_parallelism(void) {
	return 0;
}
int get_suggested_cluster_size(void) {
	return 1;
}

void *
get_key_struct(void) {
	return aes_get_key_struct();
}
void free_key_struct(void *keys) {
	aes_free_key_struct(keys);
}

void set_even_control_word(void *keys, const unsigned char *even) {
	aes_set_even_control_word(keys, even);
}

void set_odd_control_word(void *keys, const unsigned char *odd) {
	aes_set_odd_control_word(keys, odd);
}

int decrypt_packets(void *keys, unsigned char **cluster) {
	aes_decrypt_packet(keys, cluster[0]);
	return 1;
}
