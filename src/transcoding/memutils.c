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
#include <libavcodec/avcodec.h>


static char *
str_add(const char *sep, const char *str)
{
    size_t sep_len = strlen(sep), str_len = strlen(str);
    char *result = malloc(sep_len + str_len + 1);

    if (result) {
        if (str_len > 0) {
            strcpy(result, sep);
            strcat(result, str);
        } else {
            result[0] = '\0';
        }
    } else {
        tvhinfo(LS_TRANSCODE, "malloc is NULL");
    }
    return result;
}


static char *
str_vjoin(const char *separator, va_list ap)
{
    const char *sep = separator ? separator : "", *va_str = NULL;
    char *str = NULL, *tmp_result = NULL, *result = NULL;
    size_t str_len = 0, result_size = 1;

    result = calloc(1, result_size);
    if (result == NULL) {
        tvhinfo(LS_TRANSCODE, "calloc is NULL");
        return NULL;
    }
    while ((va_str = va_arg(ap, const char *))) {
        str_len = strlen(va_str);
        if (str_len == 0)
            continue;
        str = str_add(result[0] ? sep : "", va_str);
        if (str) {
            str_len = strlen(str);
            if (str_len) {
                if ((tmp_result = realloc(result, result_size + str_len))) {
                    result = tmp_result;
                    strcpy(result + result_size - 1, str);
                    result_size += str_len;
                } else {
                    tvhinfo(LS_TRANSCODE, "realloc is NULL");
                    str_clear(str);
                    goto reterr;
                }
            }
            str_clear(str);
        } else {
            goto reterr;
        }
    }
    return result;

reterr:
    free(result);
    return NULL;
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
    uint8_t *data = av_malloc(pb->pb_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (data)
        memcpy(data, pb->pb_data, pb->pb_size);
    return data;
}
