/*
 *  tvheadend, Wizard
 *  Copyright (C) 2015,2016 Jaroslav Kysela
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

#include "tvheadend.h"
#include "config.h"
#include "access.h"
#include "settings.h"
#include "input.h"
#include "wizard.h"

/*
 *
 */

static const void *empty_get(void *o)
{
  prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static const void *icon_get(void *o)
{
  strcpy(prop_sbuf, "docresources/tvheadendlogo.png");
  return &prop_sbuf_ptr;
}

#define SPECIAL_PROP(idval, getfcn) { \
  .type = PT_STR, \
  .id   = idval, \
  .name = "", \
  .get  = getfcn, \
  .opts = PO_RDONLY | PO_NOUI \
}

#define PREV_BUTTON(page) SPECIAL_PROP("page_prev_" STRINGIFY(page), empty_get)
#define NEXT_BUTTON(page) SPECIAL_PROP("page_next_" STRINGIFY(page), empty_get)
#define LAST_BUTTON()     SPECIAL_PROP("page_last", empty_get)
#define ICON()            SPECIAL_PROP("icon", icon_get)
#define DESCRIPTION(page) SPECIAL_PROP("description", wizard_description_##page)

#define DESCRIPTION_FCN(page, desc) \
static const void *wizard_description_##page(void *o) \
{ \
  static const char *t = desc; \
  return &t; \
}

#define BASIC_STR_OPS(stru, field) \
static const void *wizard_get_value_##field(void *o) \
{ \
  wizard_page_t *p = o; \
  stru *w = p->aux; \
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", w->field); \
  return &prop_sbuf_ptr; \
} \
static int wizard_set_value_##field(void *o, const void *v) \
{ \
  wizard_page_t *p = o; \
  stru *w = p->aux; \
  snprintf(w->field, sizeof(w->field), "%s", (const char *)v); \
  return 1; \
}

/*
 *
 */

static void page_free(wizard_page_t *page)
{
  free(page->aux);
  free((char *)page->idnode.in_class);
  free(page);
}

static wizard_page_t *page_init(const char *class_name, const char *caption)
{
  wizard_page_t *page = calloc(1, sizeof(*page));
  idclass_t *ic = calloc(1, sizeof(*ic));
  page->idnode.in_class = ic;
  ic->ic_caption = caption;
  ic->ic_class = ic->ic_event = class_name;
  ic->ic_perm_def = ACCESS_ADMIN;
  page->free = page_free;
  return page;
}

/*
 *
 */

static const void *hello_get_network(void *o)
{
  strcpy(prop_sbuf, "Test123");
  return &prop_sbuf_ptr;
}

static int hello_set_network(void *o, const void *v)
{
  return 0;
}

/*
 * Hello
 */

typedef struct wizard_hello {
  char ui_lang[32];
  char epg_lang1[32];
  char epg_lang2[32];
  char epg_lang3[32];
} wizard_hello_t;


static void hello_save(idnode_t *in)
{
  wizard_page_t *p = (wizard_page_t *)in;
  wizard_hello_t *w = p->aux;
  char buf[32];
  size_t l = 0;
  int save = 0;

  if (w->ui_lang[0] && strcmp(config.language_ui ?: "", w->ui_lang)) {
    free(config.language_ui);
    config.language_ui = strdup(w->ui_lang);
    save = 1;
  }
  buf[0] = '\0';
  if (w->epg_lang1[0])
    tvh_strlcatf(buf, sizeof(buf), l, "%s", w->epg_lang1);
  if (w->epg_lang2[0])
    tvh_strlcatf(buf, sizeof(buf), l, "%s%s", l > 0 ? "," : "", w->epg_lang2);
  if (w->epg_lang3[0])
    tvh_strlcatf(buf, sizeof(buf), l, "%s%s", l > 0 ? "," : "", w->epg_lang3);
  if (buf[0] && strcmp(buf, config.language ?: "")) {
    free(config.language);
    config.language = strdup(buf);
    save = 1;
  }
  if (save)
    config_save();
}

BASIC_STR_OPS(wizard_hello_t, ui_lang)
BASIC_STR_OPS(wizard_hello_t, epg_lang1)
BASIC_STR_OPS(wizard_hello_t, epg_lang2)
BASIC_STR_OPS(wizard_hello_t, epg_lang3)

DESCRIPTION_FCN(hello, N_("\
Enter the languages for the web user interface and \
for EPG texts.\
"))

wizard_page_t *wizard_hello(void)
{
  static const property_group_t groups[] = {
    {
      .name     = N_("Web interface"),
      .number   = 1,
    },
    {
      .name     = N_("EPG Language (priority order)"),
      .number   = 2,
    },
    {}
  };
  static const property_t props[] = {
    {
      .type     = PT_STR,
      .id       = "ui_lang",
      .name     = N_("Language"),
      .get      = wizard_get_value_ui_lang,
      .set      = wizard_set_value_ui_lang,
      .list     = language_get_ui_list,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "epg_lang1",
      .name     = N_("Language 1"),
      .get      = wizard_get_value_epg_lang1,
      .set      = wizard_set_value_epg_lang1,
      .list     = language_get_list,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "epg_lang2",
      .name     = N_("Language 2"),
      .get      = wizard_get_value_epg_lang2,
      .set      = wizard_set_value_epg_lang2,
      .list     = language_get_list,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "epg_lang3",
      .name     = N_("Language 3"),
      .get      = wizard_get_value_epg_lang3,
      .set      = wizard_set_value_epg_lang3,
      .list     = language_get_list,
      .group    = 2
    },
    ICON(),
    DESCRIPTION(hello),
    NEXT_BUTTON(login),
    {}
  };
  wizard_page_t *page =
    page_init("wizard_hello",
    N_("Welcome - Tvheadend - your TV streaming server and video recorder"));
  idclass_t *ic = (idclass_t *)page->idnode.in_class;
  wizard_hello_t *w;
  htsmsg_t *m;
  htsmsg_field_t *f;
  const char *s;
  int idx;

  ic->ic_properties = props;
  ic->ic_groups = groups;
  ic->ic_save = hello_save;
  page->aux = w = calloc(1, sizeof(wizard_hello_t));

  if (config.language_ui)
    strncpy(w->ui_lang, config.language_ui, sizeof(w->ui_lang)-1);

  m = htsmsg_csv_2_list(config.language, ',');
  f = m ? HTSMSG_FIRST(m) : NULL;
  for (idx = 0; idx < 3 && f != NULL; idx++) {
    s = htsmsg_field_get_string(f);
    if (s == NULL) break;
    switch (idx) {
    case 0: strncpy(w->epg_lang1, s, sizeof(w->epg_lang1) - 1); break;
    case 1: strncpy(w->epg_lang2, s, sizeof(w->epg_lang2) - 1); break;
    case 2: strncpy(w->epg_lang3, s, sizeof(w->epg_lang3) - 1); break;
    }
    f = HTSMSG_NEXT(f);
  }
  htsmsg_destroy(m);

  return page;
}

/*
 * Login/Network access
 */

typedef struct wizard_login {
  char network[256];
  char admin_username[32];
  char admin_password[32];
  char username[32];
  char password[32];
} wizard_login_t;


static void login_save(idnode_t *in)
{
  wizard_page_t *p = (wizard_page_t *)in;
  wizard_login_t *w = p->aux;
  access_entry_t *ae, *ae_next;
  passwd_entry_t *pw, *pw_next;
  htsmsg_t *conf;
  const char *s;

  for (ae = TAILQ_FIRST(&access_entries); ae; ae = ae_next) {
    ae_next = TAILQ_NEXT(ae, ae_link);
    if (ae->ae_wizard)
      access_entry_destroy(ae, 1);
  }

  for (pw = TAILQ_FIRST(&passwd_entries); pw; pw = pw_next) {
    pw_next = TAILQ_NEXT(pw, pw_link);
    if (pw->pw_wizard)
      passwd_entry_destroy(pw, 1);
  }

  s = w->admin_username[0] ? w->admin_username : "*";
  conf = htsmsg_create_map();
  htsmsg_add_bool(conf, "enabled", 1);
  htsmsg_add_str(conf, "prefix", w->network);
  htsmsg_add_str(conf, "username", s);
  htsmsg_add_str(conf, "password", w->admin_password);
  htsmsg_add_bool(conf, "streaming", 1);
  htsmsg_add_bool(conf, "adv_streaming", 1);
  htsmsg_add_bool(conf, "htsp_streaming", 1);
  htsmsg_add_bool(conf, "dvr", 1);
  htsmsg_add_bool(conf, "htsp_dvr", 1);
  htsmsg_add_bool(conf, "webui", 1);
  htsmsg_add_bool(conf, "admin", 1);
  ae = access_entry_create(NULL, conf);
  if (ae) {
    ae->ae_wizard = 1;
    access_entry_save(ae);
  }
  htsmsg_destroy(conf);

  if (s && s[0] != '*' && w->admin_password[0]) {
    conf = htsmsg_create_map();
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str(conf, "username", s);
    htsmsg_add_str(conf, "password", w->admin_password);
    pw = passwd_entry_create(NULL, conf);
    if (pw) {
      pw->pw_wizard = 1;
      passwd_entry_save(pw);
    }
  }

  if (w->username[0]) {
    s = w->username[0] ? w->username : "*";
    conf = htsmsg_create_map();
    htsmsg_add_str(conf, "prefix", w->network);
    htsmsg_add_str(conf, "username", s);
    htsmsg_add_str(conf, "password", w->password);
    ae = access_entry_create(NULL, conf);
    if (ae) {
      ae->ae_wizard = 1;
      access_entry_save(ae);
    }
    htsmsg_destroy(conf);

    if (s && s[0] != '*' && w->password && w->password[0]) {
      conf = htsmsg_create_map();
      htsmsg_add_bool(conf, "enabled", 1);
      htsmsg_add_str(conf, "username", s);
      htsmsg_add_str(conf, "password", w->password);
      pw = passwd_entry_create(NULL, conf);
      if (pw) {
        pw->pw_wizard = 1;
        passwd_entry_save(pw);
      }
    }
  }
}

BASIC_STR_OPS(wizard_login_t, network)
BASIC_STR_OPS(wizard_login_t, admin_username)
BASIC_STR_OPS(wizard_login_t, admin_password)
BASIC_STR_OPS(wizard_login_t, username)
BASIC_STR_OPS(wizard_login_t, password)

DESCRIPTION_FCN(login, N_("\
Enter the access control details to secure your system. \
The first part of this covers the IPv4 network details \
for address-based access to the system; for example, \
192.168.1.0/24 to allow local access only to 192.168.1.x clients, \
or 0.0.0.0/0 or empty value for access from any system.\n\
This works alongside the second part, which is a familiar \
username/password combination, so provide these for both \
an administrator and regular (day-to-day) user. \
You can leave the username and password blank if you don't want \
this part, and would prefer anonymous access to anyone.\n\
This wizard should be run only on the initial setup. Please, cancel \
it, if you are not willing to touch the current configuration.\
"))

wizard_page_t *wizard_login(void)
{
  static const property_group_t groups[] = {
    {
      .name     = N_("Network access"),
      .number   = 1,
    },
    {
      .name     = N_("Administrator login"),
      .number   = 2,
    },
    {
      .name     = N_("User login"),
      .number   = 3,
    },
    {}
  };
  static const property_t props[] = {
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = N_("Allowed network"),
      .get      = wizard_get_value_network,
      .set      = wizard_set_value_network,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "admin_username",
      .name     = N_("Admin username"),
      .get      = wizard_get_value_admin_username,
      .set      = wizard_set_value_admin_username,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "admin_password",
      .name     = N_("Admin password"),
      .get      = wizard_get_value_admin_password,
      .set      = wizard_set_value_admin_password,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = N_("Username"),
      .get      = wizard_get_value_username,
      .set      = wizard_set_value_username,
      .group    = 3
    },
    {
      .type     = PT_STR,
      .id       = "password",
      .name     = N_("Password"),
      .get      = wizard_get_value_password,
      .set      = wizard_set_value_password,
      .group    = 3
    },
    ICON(),
    DESCRIPTION(login),
    PREV_BUTTON(hello),
    NEXT_BUTTON(network),
    {}
  };
  wizard_page_t *page =
    page_init("wizard_login",
    N_("Welcome - Tvheadend - your TV streaming server and video recorder"));
  idclass_t *ic = (idclass_t *)page->idnode.in_class;
  wizard_login_t *w;
  access_entry_t *ae;
  passwd_entry_t *pw;

  ic->ic_properties = props;
  ic->ic_groups = groups;
  ic->ic_save = login_save;
  page->aux = w = calloc(1, sizeof(wizard_login_t));

  TAILQ_FOREACH(ae, &access_entries, ae_link) {
    if (!ae->ae_wizard)
      continue;
    if (ae->ae_admin) {
      htsmsg_t *c = htsmsg_create_map();
      idnode_save(&ae->ae_id, c);
      snprintf(w->admin_username, sizeof(w->admin_username), "%s", ae->ae_username);
      snprintf(w->network, sizeof(w->network), "%s", htsmsg_get_str(c, "prefix") ?: "");
      htsmsg_destroy(c);
    } else {
      snprintf(w->username, sizeof(w->username), "%s", ae->ae_username);
    }
  }

  TAILQ_FOREACH(pw, &passwd_entries, pw_link) {
    if (!pw->pw_wizard || !pw->pw_username)
      continue;
    if (w->admin_username[0] &&
        strcmp(w->admin_username, pw->pw_username) == 0) {
      snprintf(w->admin_password, sizeof(w->admin_password), "%s", pw->pw_password);
    } else if (w->username[0] && strcmp(w->username, pw->pw_username) == 0) {
      snprintf(w->password, sizeof(w->password), "%s", pw->pw_password);
    }
  }

  return page;
}

/*
 * Network settings
 */

typedef struct wizard_network {
  char network_type1[32];
  char network_type2[32];
  char network_type3[32];
  char network_type4[32];
} wizard_network_t;

#define NETWORK(num, nameval) { \
  .type = PT_STR, \
  .id   = "network" STRINGIFY(num), \
  .name = nameval, \
  .get  = network_get_value##num, \
  .set  = network_set_value##num, \
  .list = network_get_list, \
}

#define NETWORK_FCN(num) \
static const void *network_get_value##num(void *o) \
{ \
  wizard_page_t *p = o; \
  wizard_network_t *w = p->aux; \
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", w->network_type##num); \
  return &prop_sbuf_ptr; \
} \
static int network_set_value##num(void *o, const void *v) \
{ \
  wizard_page_t *p = o; \
  wizard_network_t *w = p->aux; \
  snprintf(w->network_type##num, sizeof(w->network_type##num), "%s", (const char *)v); \
  return 1; \
}

NETWORK_FCN(1)
NETWORK_FCN(2)
NETWORK_FCN(3)
NETWORK_FCN(4)

DESCRIPTION_FCN(network, N_("\
Create networks. The T means terresterial, C is cable and S is satellite.\
"))

static htsmsg_t *network_get_list(void *o, const char *lang)
{
  mpegts_network_builder_t *mnb;
  htsmsg_t *e, *l = htsmsg_create_list();
  LIST_FOREACH(mnb, &mpegts_network_builders, link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", mnb->idc->ic_class);
    htsmsg_add_str(e, "val", idclass_get_caption(mnb->idc, lang));
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

wizard_page_t *wizard_network(void)
{
  static const property_t props[] = {
    NETWORK(1, N_("Network 1")),
    NETWORK(2, N_("Network 2")),
    NETWORK(3, N_("Network 3")),
    NETWORK(4, N_("Network 4")),
    ICON(),
    DESCRIPTION(network),
    PREV_BUTTON(login),
    NEXT_BUTTON(input),
    {}
  };
  wizard_page_t *page = page_init("wizard_network", N_("Network settings"));
  idclass_t *ic = (idclass_t *)page->idnode.in_class;
  ic->ic_properties = props;
  page->aux = calloc(1, sizeof(wizard_network_t));
  return page;
}

/*
 * Input settings
 */

DESCRIPTION_FCN(input, N_("\
Assign inputs to networks.\
"))


wizard_page_t *wizard_input(void)
{
  static const property_t props[] = {
    {
      .type     = PT_STR,
      .id       = "input1",
      .name     = N_("Input 1 network"),
      .get      = hello_get_network,
      .set      = hello_set_network,
    },
    {
      .type     = PT_STR,
      .id       = "input2",
      .name     = N_("Input 2 network"),
      .get      = hello_get_network,
      .set      = hello_set_network,
    },
    {
      .type     = PT_STR,
      .id       = "input3",
      .name     = N_("Input 3 network"),
      .get      = hello_get_network,
      .set      = hello_set_network,
    },
    ICON(),
    DESCRIPTION(input),
    PREV_BUTTON(network),
    NEXT_BUTTON(status),
    {}
  };
  wizard_page_t *page = page_init("wizard_input", N_("Input / tuner settings"));
  idclass_t *ic = (idclass_t *)page->idnode.in_class;
  ic->ic_properties = props;
  return page;
}

/*
 * Status
 */

DESCRIPTION_FCN(status, N_("\
Show the scan status.\
"))


wizard_page_t *wizard_status(void)
{
  static const property_t props[] = {
    {
      .type     = PT_STR,
      .id       = "muxes",
      .name     = N_("Found muxes"),
      .desc     = N_("Number of muxes found."),
      .get      = hello_get_network,
      .set      = hello_set_network,
    },
    {
      .type     = PT_STR,
      .id       = "services",
      .name     = N_("Found services"),
      .desc     = N_("Total number of services found."),
      .get      = hello_get_network,
      .set      = hello_set_network,
    },
    ICON(),
    DESCRIPTION(status),
    PREV_BUTTON(input),
    NEXT_BUTTON(mapping),
    {}
  };
  wizard_page_t *page = page_init("wizard_status", N_("Scan status"));
  idclass_t *ic = (idclass_t *)page->idnode.in_class;
  ic->ic_properties = props;
  return page;
}

/*
 * Service Mapping
 */

DESCRIPTION_FCN(mapping, N_("\
Do the service mapping to channels.\
"))


wizard_page_t *wizard_mapping(void)
{
  static const property_t props[] = {
    {
      .type     = PT_STR,
      .id       = "pnetwork",
      .name     = N_("Select network"),
      .desc     = N_("Select a Network."),
      .get      = hello_get_network,
      .set      = hello_set_network,
    },
    ICON(),
    DESCRIPTION(mapping),
    PREV_BUTTON(status),
    LAST_BUTTON(),
    {}
  };
  wizard_page_t *page = page_init("wizard_service_map", N_("Service mapping"));
  idclass_t *ic = (idclass_t *)page->idnode.in_class;
  ic->ic_properties = props;
  return page;
}
