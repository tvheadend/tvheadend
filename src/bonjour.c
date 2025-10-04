/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Damjan Marion
 *
 * Bonjour service publisher
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tvheadend.h"
#include "bonjour.h"

#include <CoreServices/CoreServices.h>

typedef struct {
  const char *key;
  const char *value;
} txt_rec_t;

pthread_t bonjour_tid;
CFNetServiceRef svc_http, svc_htsp;

static void
bonjour_callback(CFNetServiceRef theService, CFStreamError* error, void* info)
{  
  if (error->error) {
    tvherror(LS_BONJOUR, "callback error (domain = %ld, error =%d)",
             error->domain, error->error);   
  } 
}

static void
bonjour_start_service(CFNetServiceRef *svc, const char *service_type,
                      uint32_t port, txt_rec_t *txt)
{
  CFStringRef str;
  CFStreamError error = {0};
  CFNetServiceClientContext context = {0, NULL, NULL, NULL, NULL};
  
  str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, service_type,
                                        kCFStringEncodingASCII,
                                        kCFAllocatorNull);

  *svc = CFNetServiceCreate(NULL, CFSTR(""), str, CFSTR("Tvheadend"), port);
  if (!*svc) {
    tvherror(LS_BONJOUR, "service creation failed"); 
    return;
  }

  CFNetServiceSetClient(*svc, bonjour_callback, &context);
  CFNetServiceScheduleWithRunLoop(*svc, CFRunLoopGetCurrent(),
                                  kCFRunLoopCommonModes);

  if (txt) {
    CFDataRef data = NULL;
    CFMutableDictionaryRef dict;
    dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    
    while(txt->key) {
      str = CFStringCreateWithCString (NULL, txt->key, kCFStringEncodingASCII);
      data = CFDataCreate (NULL, (uint8_t *) txt->value, strlen(txt->value));
      CFDictionaryAddValue(dict, str, data);
      txt++;
    }
    
    data = CFNetServiceCreateTXTDataWithDictionary(NULL, dict);
    CFNetServiceSetTXTData(*svc, data);
    CFRelease(data);
    CFRelease(dict);
  }

  if (!CFNetServiceRegisterWithOptions(*svc, 0, &error))
    tvherror(LS_BONJOUR, "registration failed (service type = %s, "
             "domain = %ld, error =%d)", service_type, error.domain, error.error); 
  else
    tvherror(LS_BONJOUR, "service '%s' successfully established",
             service_type);
}

static void
bonjour_stop_service(CFNetServiceRef *svc)
{
  CFNetServiceUnscheduleFromRunLoop(*svc, CFRunLoopGetCurrent(), 
                                    kCFRunLoopCommonModes);
  CFNetServiceSetClient(*svc, NULL, NULL);  
  CFRelease(*svc);
}

void
bonjour_init(void)
{
  txt_rec_t txt_rec_http[] = {
    { "path", tvheadend_webroot ? tvheadend_webroot : "/" },
    { .key = NULL }
  };

  if (tvheadend_webui_port > 0)
    bonjour_start_service(&svc_http, "_http._tcp", tvheadend_webui_port,
                          txt_rec_http);

  if (tvheadend_htsp_port > 0)
    bonjour_start_service(&svc_htsp, "_htsp._tcp", tvheadend_htsp_port, NULL);
}

void
bonjour_done(void)
{
  if (tvheadend_webui_port > 0)
    bonjour_stop_service(&svc_http);
  if (tvheadend_htsp_port > 0)
    bonjour_stop_service(&svc_htsp);
}
