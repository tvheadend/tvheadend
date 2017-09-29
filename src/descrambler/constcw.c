/*
 *  tvheadend, constant code word interface
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include <ctype.h>
#include "tvheadend.h"
#include "caclient.h"
#include "service.h"
#include "input.h"

/**
 *
 */
typedef struct constcw_service {
  th_descrambler_t;  
  LIST_ENTRY(constcw_service) cs_link;
} constcw_service_t;

/**
 *
 */
typedef struct constcw {
  caclient_t;

  /* From configuration */
  uint16_t ccw_caid;           /* CA ID */
  uint32_t ccw_providerid;     /* CA provider ID */
  uint16_t ccw_tsid;           /* transponder ID */
  uint16_t ccw_sid;            /* service ID */
  uint8_t  ccw_key_even[16];   /* DES or AES key */
  uint8_t  ccw_key_odd [16];   /* DES or AES key */
  LIST_HEAD(, constcw_service) ccw_services; /* active services */
} constcw_t;

/*
 *
 */
static const char *
constcw_name(constcw_t *ccw)
{
  return idnode_get_title(&ccw->cac_id, NULL);
}

/**
 *
 */
static int
constcw_algo(caclient_t *cac)
{
  constcw_t *ccw = (constcw_t *)cac;

  if (idnode_is_instance(&ccw->cac_id, &caclient_ccw_des_ncb_class))
    return DESCRAMBLER_DES_NCB;
  if (idnode_is_instance(&ccw->cac_id, &caclient_ccw_aes_ecb_class))
    return DESCRAMBLER_AES_ECB;
  if (idnode_is_instance(&ccw->cac_id, &caclient_ccw_aes128_ecb_class))
    return DESCRAMBLER_AES128_ECB;
  return DESCRAMBLER_CSA_CBC;
}

/**
 *
 */
static int
constcw_key_size(caclient_t *cac)
{
  constcw_t *ccw = (constcw_t *)cac;

  if (idnode_is_instance(&ccw->cac_id, &caclient_ccw_aes128_ecb_class))
    return 16;
  return 8;
}

/*
 *
 */
static int
constcw_ecm_reset(th_descrambler_t *th)
{
  return 1;
}

/**
 * s_stream_mutex is held
 */
static void 
constcw_service_destroy(th_descrambler_t *td)
{
  constcw_service_t *ct = (constcw_service_t *)td; 
  
  LIST_REMOVE(td, td_service_link);
  LIST_REMOVE(ct, cs_link);
  free(ct->td_nicename);
  free(ct);
}   

/**
 * global_lock is held. Not that we care about that, but either way, it is.
 */
static void
constcw_service_start(caclient_t *cac, service_t *t)
{
  constcw_t *ccw = (constcw_t *)cac;
  constcw_service_t *ct;
  th_descrambler_t *td;
  elementary_stream_t *st;
  mpegts_service_t *mt;
  char buf[128];
  caid_t *c;

  extern const idclass_t mpegts_service_class;
  if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
    return;
  mt = (mpegts_service_t *)t;

  if (mt->s_dvb_forcecaid && mt->s_dvb_forcecaid != ccw->ccw_caid)
    return;

  if (mt->s_dvb_service_id != ccw->ccw_sid)
    return;

  if (mt->s_dvb_mux->mm_tsid != ccw->ccw_tsid)
    return;

  LIST_FOREACH(ct, &ccw->ccw_services, cs_link)
    if (ct->td_service == t)
      break;
  if (ct)
    return;

  if (!mt->s_dvb_forcecaid) {
    pthread_mutex_lock(&t->s_stream_mutex);
    TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
      LIST_FOREACH(c, &st->es_caids, link) {
        if (c->use && c->caid == ccw->ccw_caid &&
            c->providerid == ccw->ccw_providerid)
          break;
      }
      if (c) break;
    }
    pthread_mutex_unlock(&t->s_stream_mutex);
    if (st == NULL)
      return;
  }

  ct                   = calloc(1, sizeof(constcw_service_t));
  td                   = (th_descrambler_t *)ct;
  snprintf(buf, sizeof(buf), "constcw-%s", constcw_name(ccw));
  td->td_nicename      = strdup(buf);
  td->td_service       = t;
  td->td_stop          = constcw_service_destroy;
  td->td_ecm_reset     = constcw_ecm_reset;
  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&ccw->ccw_services, ct, cs_link);

  descrambler_keys(td, constcw_algo(cac),
                   0, ccw->ccw_key_even, ccw->ccw_key_odd);
}


/**
 *
 */
static void
constcw_free(caclient_t *cac)
{
  constcw_t *ccw = (constcw_t *)cac;
  constcw_service_t *ct;

  while((ct = LIST_FIRST(&ccw->ccw_services)) != NULL) {
    service_t *t = ct->td_service;
    pthread_mutex_lock(&t->s_stream_mutex);
    constcw_service_destroy((th_descrambler_t *)ct);
    pthread_mutex_unlock(&t->s_stream_mutex);
  }
}

/**
 *
 */
static int
nibble(char c)
{
  switch(c) {
  case '0' ... '9':
    return c - '0';
  case 'a' ... 'f':
    return c - 'a' + 10;
  case 'A' ... 'F':
    return c - 'A' + 10;
  default:
    return 0;
  }
}

/**
 *
 */
static void
constcw_conf_changed(caclient_t *cac)
{
  if (cac->cac_enabled) {
    caclient_set_status(cac, CACLIENT_STATUS_CONNECTED);
  } else {
    caclient_set_status(cac, CACLIENT_STATUS_NONE);
  }
}

/**
 *
 */
static int
constcw_class_key_set(void *o, const void *v, uint8_t *dkey)
{
  const char *s = v ?: "";
  int keysize = constcw_key_size(o);
  char key[16];
  int i, u, l;

  for(i = 0; i < keysize; i++) {
    while(*s != 0 && !isxdigit(*s)) s++;
    u = *s ? nibble(*s++) : 0;
    while(*s != 0 && !isxdigit(*s)) s++;
    l = *s ? nibble(*s++) : 0;
    key[i] = (u << 4) | l;
  }
  i = memcmp(dkey, key, keysize) != 0;
  memcpy(dkey, key, keysize);
  return i;
}

static int
constcw_class_key_even_set(void *o, const void *v)
{
  constcw_t *ccw = o;
  return constcw_class_key_set(o, v, ccw->ccw_key_even);
}

static int
constcw_class_key_odd_set(void *o, const void *v)
{
  constcw_t *ccw = o;
  return constcw_class_key_set(o, v, ccw->ccw_key_odd);
}

static const void *
constcw_class_key_get(void *o, const uint8_t *key)
{
  if (constcw_key_size(o) == 8) {
    snprintf(prop_sbuf, PROP_SBUF_LEN,
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             key[0x0], key[0x1], key[0x2], key[0x3],
             key[0x4], key[0x5], key[0x6], key[0x7]);
  } else {
    snprintf(prop_sbuf, PROP_SBUF_LEN,
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             key[0x0], key[0x1], key[0x2], key[0x3],
             key[0x4], key[0x5], key[0x6], key[0x7],
             key[0x8], key[0x9], key[0xa], key[0xb],
             key[0xc], key[0xd], key[0xe], key[0xf]);
  }
  return &prop_sbuf_ptr;
}

static const void *
constcw_class_key_even_get(void *o)
{
  constcw_t *ccw = o;
  return constcw_class_key_get(o, ccw->ccw_key_even);
}

static const void *
constcw_class_key_odd_get(void *o)
{
  constcw_t *ccw = o;
  return constcw_class_key_get(o, ccw->ccw_key_odd);
}

const idclass_t caclient_ccw_csa_cbc_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_ccw_csa_cbc",
  .ic_caption    = N_("CSA CBC Constant Code Word"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U16,
      .id       = "caid",
      .name     = N_("CA ID"),
      .desc     = N_("Conditional Access Identification."),
      .off      = offsetof(constcw_t, ccw_caid),
      .opts     = PO_HEXA,
      .def.u16  = 0x2600
    },
    {
      .type     = PT_U32,
      .id       = "providerid",
      .name     = N_("Provider ID"),
      .desc     = N_("The provider's ID."),
      .off      = offsetof(constcw_t, ccw_providerid),
      .opts     = PO_HEXA,
      .def.u32  = 0
    },
    {
      .type     = PT_U16,
      .id       = "tsid",
      .name     = N_("Transponder ID"),
      .desc     = N_("The transponder ID."),
      .off      = offsetof(constcw_t, ccw_tsid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Service ID"),
      .desc     = N_("The service ID."),
      .off      = offsetof(constcw_t, ccw_sid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_STR,
      .id       = "key_even",
      .name     = N_("Even key"),
      .desc     = N_("Even key."),
      .set      = constcw_class_key_even_set,
      .get      = constcw_class_key_even_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00",
    },
    {
      .type     = PT_STR,
      .id       = "key_odd",
      .name     = N_("Odd key"),
      .desc     = N_("Odd key."),
      .set      = constcw_class_key_odd_set,
      .get      = constcw_class_key_odd_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00",
    },
    { }
  }
};

const idclass_t caclient_ccw_des_ncb_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_ccw_des_ncb",
  .ic_caption    = N_("DES NCB Constant Code Word"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U16,
      .id       = "caid",
      .name     = N_("CA ID"),
      .desc     = N_("Conditional Access Identification."),
      .off      = offsetof(constcw_t, ccw_caid),
      .opts     = PO_HEXA,
      .def.u16  = 0x2600
    },
    {
      .type     = PT_U32,
      .id       = "providerid",
      .name     = N_("Provider ID"),
      .desc     = N_("The provider's ID."),
      .off      = offsetof(constcw_t, ccw_providerid),
      .opts     = PO_HEXA,
      .def.u32  = 0
    },
    {
      .type     = PT_U16,
      .id       = "tsid",
      .name     = N_("Transponder ID"),
      .desc     = N_("The transponder ID."),
      .off      = offsetof(constcw_t, ccw_tsid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Service ID"),
      .desc     = N_("The service ID."),
      .off      = offsetof(constcw_t, ccw_sid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_STR,
      .id       = "key_even",
      .name     = N_("Even key"),
      .desc     = N_("Even key."),
      .set      = constcw_class_key_even_set,
      .get      = constcw_class_key_even_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00",
    },
    {
      .type     = PT_STR,
      .id       = "key_odd",
      .name     = N_("Odd key"),
      .desc     = N_("Odd key."),
      .set      = constcw_class_key_odd_set,
      .get      = constcw_class_key_odd_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00",
    },
    { }
  }
};

const idclass_t caclient_ccw_aes_ecb_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_ccw_aes_ecb",
  .ic_caption    = N_("AES ECB Constant Code Word"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U16,
      .id       = "caid",
      .name     = N_("CA ID"),
      .desc     = N_("Conditional Access Identification."),
      .off      = offsetof(constcw_t, ccw_caid),
      .opts     = PO_HEXA,
      .def.u16  = 0x2600
    },
    {
      .type     = PT_U32,
      .id       = "providerid",
      .name     = N_("Provider ID"),
      .desc     = N_("The provider's ID."),
      .off      = offsetof(constcw_t, ccw_providerid),
      .opts     = PO_HEXA,
      .def.u32  = 0
    },
    {
      .type     = PT_U16,
      .id       = "tsid",
      .name     = N_("Transponder ID"),
      .desc     = N_("The transponder ID."),
      .off      = offsetof(constcw_t, ccw_tsid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Service ID"),
      .desc     = N_("The service ID."),
      .off      = offsetof(constcw_t, ccw_sid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_STR,
      .id       = "key_even",
      .name     = N_("Even key"),
      .desc     = N_("Even key."),
      .set      = constcw_class_key_even_set,
      .get      = constcw_class_key_even_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00",
    },
    {
      .type     = PT_STR,
      .id       = "key_odd",
      .name     = N_("Odd key"),
      .desc     = N_("Odd key."),
      .set      = constcw_class_key_odd_set,
      .get      = constcw_class_key_odd_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00",
    },
    { }
  }
};

const idclass_t caclient_ccw_aes128_ecb_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_ccw_aes128_ecb",
  .ic_caption    = N_("AES128 ECB Constant Code Word"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U16,
      .id       = "caid",
      .name     = N_("CA ID"),
      .desc     = N_("Conditional Access Identification."),
      .off      = offsetof(constcw_t, ccw_caid),
      .opts     = PO_HEXA,
      .def.u16  = 0x2600,
    },
    {
      .type     = PT_U32,
      .id       = "providerid",
      .name     = N_("Provider ID"),
      .desc     = N_("The provider's ID."),
      .off      = offsetof(constcw_t, ccw_providerid),
      .opts     = PO_HEXA,
      .def.u32  = 0
    },
    {
      .type     = PT_U16,
      .id       = "tsid",
      .name     = N_("Transponder ID"),
      .desc     = N_("The transponder ID."),
      .off      = offsetof(constcw_t, ccw_tsid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Service ID"),
      .desc     = N_("The service ID"),
      .off      = offsetof(constcw_t, ccw_sid),
      .opts     = PO_HEXA,
      .def.u16  = 1,
    },
    {
      .type     = PT_STR,
      .id       = "key_even",
      .name     = N_("Even key"),
      .desc     = N_("Even key."),
      .set      = constcw_class_key_even_set,
      .get      = constcw_class_key_even_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00",
    },
    {
      .type     = PT_STR,
      .id       = "key_odd",
      .name     = N_("Odd key"),
      .desc     = N_("Odd key."),
      .set      = constcw_class_key_odd_set,
      .get      = constcw_class_key_odd_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00",
    },
    { }
  }
};

/*
 *
 */
caclient_t *constcw_create(void)
{
  constcw_t *ccw = calloc(1, sizeof(*ccw));

  ccw->cac_free         = constcw_free;
  ccw->cac_start        = constcw_service_start;
  ccw->cac_conf_changed = constcw_conf_changed;
  return (caclient_t *)ccw;
}
