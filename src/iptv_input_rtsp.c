/*
 *  RTSP IPTV Input
 *  Copyright (C) 2012
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

#include "iptv_input_rtsp.h"

#include <curl/curl.h>
#include <string.h>
#include <ctype.h>
#include <sys/queue.h>

#include "tvheadend.h"

struct iptv_rtsp_info
{
  CURL *curl;
  const char *uri;
};

/**
 * Define a simple header value with name and value, without ending \r\n
 * The response code isn't included, since curl keeps it as an internal value.
 */
typedef struct curl_header
{
  char *name;
  char *value;
  LIST_ENTRY(curl_header) next;
} curl_header_t;

/**
 * The whole response, with a linked-list for headers and a raw string for data.
 */
typedef struct curl_response
{
  LIST_HEAD(, curl_header) header_head;
  char *data;
} curl_response_t;

/* the function to invoke as the data recieved */
size_t static
curl_data_write_func(void *buffer, size_t size, size_t nmemb, void *userp)
{
  curl_response_t *response_ptr = (curl_response_t *)userp;

  /* assuming the response is a string */
  response_ptr->data = strndup(buffer, (size_t)(size * nmemb));
  
  return size * nmemb;
}

size_t static
curl_header_write_func(void *buffer, size_t size, size_t nmemb, void *userp)
{
  size_t used = size * nmemb;
  char *raw_header = (char *)buffer;
  char *separator = strstr(buffer, ": ");
  if(separator)
  {
    curl_response_t *response_ptr =  (curl_response_t *)userp;

    curl_header_t *header = malloc(sizeof(curl_header_t));
    size_t position = separator - raw_header;
    header->name = strndup(buffer, position);
    
    /* Trim the value */
    separator += 2;
    while(isspace(*separator))
    {
      ++separator;
    }
    position = separator - raw_header;
    size_t value_size = (size_t)(size * nmemb) - position;
    /* Trim the ending whitespaces and return chars */
    while(isspace(*(separator + value_size - 1)))
    {
      --value_size;
    }
    header->value = strndup(separator, value_size);

    LIST_INSERT_HEAD(&response_ptr->header_head, header, next);
  }
  
  return used;
}

static void
clean_header_list(curl_response_t *response)
{
  curl_header_t *header;
  while (! LIST_EMPTY(&response->header_head))
  {
    header = LIST_FIRST(&response->header_head);
    LIST_REMOVE(header, next);
    free(header);
  }

}

static void
set_curl_result_buffer(CURL *curl, curl_response_t *buffer)
{
  if(buffer)
  {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_data_write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, buffer);
    clean_header_list(buffer);
    if(buffer->data)
    {
      free(buffer->data);
      buffer->data = NULL;
    }
  }
}

 
/* send RTSP OPTIONS request */ 
static CURLcode
rtsp_options(CURL *curl, const char *uri, curl_response_t *response)
{
  CURLcode result;
  result = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  if(result != CURLE_OK)
  {
    return result;
  }
  result = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_OPTIONS);
  if (result != CURLE_OK)
  {
    return result;
  }
  set_curl_result_buffer(curl, response);
  return curl_easy_perform(curl);
}
 
/* send RTSP DESCRIBE request */ 
static CURLcode
rtsp_describe(CURL *curl, curl_response_t *response)
{
  CURLcode result;
  result = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_DESCRIBE);
  if(result != CURLE_OK)
  {
    return result;
  }
  set_curl_result_buffer(curl, response);
  return curl_easy_perform(curl);
}
 
/* send RTSP SETUP request */ 
static CURLcode
rtsp_setup(CURL *curl, const char *uri, const char *transport, curl_response_t *response)
{
  CURLcode result;
  result = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  if(result != CURLE_OK)
  {
    return result;
  }
  result = curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport);
  if(result != CURLE_OK)
  {
    return result;
  }
  result = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_SETUP);
  if(result != CURLE_OK)
  {
    return result;
  }
  set_curl_result_buffer(curl, response);
  return curl_easy_perform(curl);
}
 
 
/* send RTSP PLAY request */ 
static CURLcode
rtsp_play(CURL *curl, const char *uri, const char *range, curl_response_t *response)
{
  CURLcode result;
  result = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  if(result != CURLE_OK)
  {
    return result;
  }
  result = curl_easy_setopt(curl, CURLOPT_RANGE, range);
  if(result != CURLE_OK)
  {
    return result;
  }
  result = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_PLAY);
  if(result != CURLE_OK)
  {
    return result;
  }
  set_curl_result_buffer(curl, response);
  return curl_easy_perform(curl);
}
 
 
/* send RTSP TEARDOWN request */ 
static CURLcode
rtsp_teardown(CURL *curl, const char *uri, curl_response_t *response)
{
  CURLcode result;
  result = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_TEARDOWN);
  if(result != CURLE_OK)
  {
    return result;
  }
  set_curl_result_buffer(curl, response);
  return curl_easy_perform(curl);
}

static CURL*
curl_init()
{
  CURL *curl = curl_easy_init();
  if (curl != NULL) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  }
  return curl;
}

static curl_response_t *
create_response()
{
  curl_response_t *response = calloc(1, sizeof(curl_response_t));
  LIST_INIT(&response->header_head);
  return response;
}

static void
destroy_response(curl_response_t *response)
{
  clean_header_list(response);
  if(response->data)
  {
    free(response->data);
    response->data = NULL;
  }
  free(response);
  response = NULL;
}

iptv_rtsp_info_t *
iptv_rtsp_start(const char *uri)
{
  /* initialize this curl session */
  CURL *curl = curl_init();
  curl_response_t *response = create_response();
  iptv_rtsp_info_t *rtsp_info;
  CURLcode result;
  
  result = curl_easy_setopt(curl, CURLOPT_URL, uri);
  tvhlog(LOG_DEBUG, "IPTV", "cURL init : %d", result);
  
  result = rtsp_options(curl, uri, response);
  curl_header_t *header = response->header_head.lh_first->next.le_next;
  tvhlog(LOG_DEBUG, "IPTV", "RTSP OPTIONS answer : %d, %s: %s", result, header->name, header->value);
  
  result = rtsp_describe(curl, response);
  tvhlog(LOG_DEBUG, "IPTV", "RTSP DESCRIBE answer : %d, %s", result, response->data);

  result = rtsp_setup(curl, uri, "RTP/AVP;unicast;client_port=1234-1235", response);
  header = response->header_head.lh_first->next.le_next;
  tvhlog(LOG_DEBUG, "IPTV", "RTSP SETUP answer : %d, %s: %s", result, header->name, header->value);
  
  result = rtsp_play(curl, uri, "0.000-", response);
  header = response->header_head.lh_first->next.le_next;
  tvhlog(LOG_DEBUG, "IPTV", "RTSP PLAY answer : %d, %s: %s", result, header->name, header->value);
  
  destroy_response(response);
  
  rtsp_info = malloc(sizeof(iptv_rtsp_info_t));
  rtsp_info->curl = curl;
  rtsp_info->uri = uri;
  
  return rtsp_info;
  
  result = rtsp_teardown(curl, uri, response);
}

void
iptv_rtsp_stop(iptv_rtsp_info_t *rtsp_info)
{
  rtsp_teardown(rtsp_info->curl, rtsp_info->uri, NULL);
  
  curl_easy_cleanup(rtsp_info->curl);
  free(rtsp_info);
  rtsp_info = NULL;
}

int
iptv_init_rtsp()
{
  // Init cURL
  // TODO: call curl_global_cleanup at exit
  curl_global_init(CURL_GLOBAL_ALL);
  return 0;
}