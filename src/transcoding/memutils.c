/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2016 Tvheadend
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "memutils.h"

#include <libavutil/mem.h>


static char *
str_add(const char *sep, const char *str)
{
    size_t sep_len = strlen(sep), str_len = strlen(str);
    char *result = NULL;

    if ((result = calloc(1, sep_len + str_len + 1))) {
        if (str_len > 0) {
            strncpy(result, sep, sep_len);
            strncat(result, str, str_len);
        }
    }
    return result;
}


static char *
str_vjoin(const char *separator, va_list ap)
{
    const char *sep = separator ? separator : "", *va_str = NULL;
    char *str = NULL, *tmp_result = NULL, *result = NULL;
    size_t str_len = 0, result_size = 1;

    if ((result = calloc(1, result_size))) {
        while ((va_str = va_arg(ap, const char *))) {
            if (strlen(va_str)) {
                if ((str = str_add(strlen(result) ? sep : "", va_str))) {
                    if ((str_len = strlen(str))) {
                        result_size += str_len;
                        if ((tmp_result = realloc(result, result_size))) {
                            result = tmp_result;
                            strncat(result, str, str_len);
                        }
                        else {
                            str_clear(result);
                        }
                    }
                    str_clear(str);
                }
                else {
                    str_clear(result);
                }
                if (!result) {
                    break;
                }
            }
        }
    }
    return result;
}


char *
str_join(const char *separator, ...)
{
    va_list ap;
    va_start(ap, separator);
    char *result = str_vjoin(separator, ap);
    va_end(ap);
    return result;
}


int
str_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return (ret < 0 || ret >= size) ? -1 : 0;
}

/* _IMPORTANT!_: need to check for pb->pb_size and pb->pb_data
   _BEFORE_ calling pktbuf_copy_data */
uint8_t *
pktbuf_copy_data(pktbuf_t *pb)
{
    uint8_t *data = av_mallocz(pb->pb_size);
    if (data) {
        memcpy(data, pb->pb_data, pb->pb_size);
    }
    return data;
}
