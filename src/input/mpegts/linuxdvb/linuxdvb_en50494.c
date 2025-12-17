/*
 *  Tvheadend - Linux DVB EN50494
 *              (known under trademark "UniCable")
 *
 *  Copyright (C) 2013 Sascha "InuSasha" Kuehndel
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
 *
 *  Open things:
 *    - TODO: collision detection
 *      * compare transport-stream-id from stream with id in config
 *      * check continuity of the pcr-counter
 *      * when one point is given -> retry
 *      * delay time is easily random, but in standard is special (complicated) way described (cap. 8).
 */

#include "tvheadend.h"
#include "linuxdvb_private.h"
#include "settings.h"

#include <unistd.h>
#include <math.h>
#include <fcntl.h>

#include <linux/dvb/frontend.h>

/* **************************************************************************
 * Static definition
 * *************************************************************************/

#define LINUXDVB_EN50494_NOPIN                 256

#define LINUXDVB_EN50494_FRAME                 0xE0
/* addresses 0x00, 0x10 and 0x11 are possible */
#define LINUXDVB_EN50494_ADDRESS               0x10

#define LINUXDVB_EN50494_CMD_NORMAL            0x5A
#define LINUXDVB_EN50494_CMD_NORMAL_MULTIHOME  0x5C
/* special modes not implemented yet */
#define LINUXDVB_EN50494_CMD_SPECIAL           0x5B
#define LINUXDVB_EN50494_CMD_SPECIAL_MULTIHOME 0x5D

#define LINUXDVB_EN50494_SAT_A                 0x00
#define LINUXDVB_EN50494_SAT_B                 0x01

/* EN50607 */
#define LINUXDVB_EN50607_FRAME_NORMAL          0x70
#define LINUXDVB_EN50607_FRAME_MULTIHOME       0x71

/* **************************************************************************
 * Class definition
 * *************************************************************************/

/* prevention of self raised DiSEqC collisions */
static tvh_mutex_t linuxdvb_en50494_lock;

/* Unicable master/slave group management */
static linuxdvb_unicable_group_list_t linuxdvb_unicable_groups;
static tvh_mutex_t linuxdvb_unicable_groups_lock;

/*
 * Find master satconf element for a unicable group
 * Must be called WITHOUT groups_lock held
 */
static linuxdvb_satconf_ele_t *
linuxdvb_unicable_find_master ( uint16_t group_id )
{
  tvh_hardware_t *th;
  linuxdvb_adapter_t *la;
  linuxdvb_frontend_t *lfe;
  linuxdvb_satconf_ele_t *lse;

  LIST_FOREACH(th, &tvh_hardware, th_link) {
    if (!idnode_is_instance(&th->th_id, &linuxdvb_adapter_class))
      continue;
    la = (linuxdvb_adapter_t *)th;
    LIST_FOREACH(lfe, &la->la_frontends, lfe_link) {
      if (lfe->lfe_type != DVB_TYPE_S || !lfe->lfe_satconf)
        continue;
      TAILQ_FOREACH(lse, &lfe->lfe_satconf->ls_elements, lse_link) {
        if (!lse->lse_en50494)
          continue;
        linuxdvb_en50494_t *le = (linuxdvb_en50494_t *)lse->lse_en50494;
        if (le->le_group_id == group_id && le->le_is_master)
          return lse;
      }
    }
  }
  return NULL;
}

/*
 * Find or create a unicable group by ID
 */
static linuxdvb_unicable_group_t *
linuxdvb_unicable_group_get ( uint16_t group_id )
{
  linuxdvb_unicable_group_t *group;

  if (group_id == 0)
    return NULL;  /* group 0 means standalone, no master/slave */

  tvh_mutex_lock(&linuxdvb_unicable_groups_lock);

  /* Search for existing group */
  TAILQ_FOREACH(group, &linuxdvb_unicable_groups, lug_link) {
    if (group->lug_group_id == group_id) {
      tvh_mutex_unlock(&linuxdvb_unicable_groups_lock);
      return group;
    }
  }

  /* Create new group */
  group = calloc(1, sizeof(*group));
  if (group) {
    group->lug_group_id = group_id;
    group->lug_master_fd = -1;
    group->lug_fd_owned = 0;
    tvh_mutex_init(&group->lug_lock, NULL);
    TAILQ_INSERT_TAIL(&linuxdvb_unicable_groups, group, lug_link);
    tvhtrace(LS_EN50494, "created unicable group %d", group_id);
  }

  tvh_mutex_unlock(&linuxdvb_unicable_groups_lock);
  return group;
}

/*
 * Get master frontend FD for a group - opens if necessary
 * Must be called WITH group->lug_lock held
 */
static int
linuxdvb_unicable_get_master_fd ( linuxdvb_unicable_group_t *group )
{
  linuxdvb_satconf_t *ls;
  linuxdvb_frontend_t *lfe;

  /* Return cached FD if valid */
  if (group->lug_master_fd > 0)
    return group->lug_master_fd;

  /* Find master element if not cached */
  if (!group->lug_master_ele) {
    group->lug_master_ele = linuxdvb_unicable_find_master(group->lug_group_id);
    if (!group->lug_master_ele) {
      tvherror(LS_EN50494, "no master found for unicable group %d", group->lug_group_id);
      return -1;
    }
  }

  ls = group->lug_master_ele->lse_parent;
  lfe = (linuxdvb_frontend_t *)ls->ls_frontend;

  /* Use existing FD if master frontend is active */
  if (lfe->lfe_fe_fd > 0) {
    group->lug_master_fd = lfe->lfe_fe_fd;
    group->lug_fd_owned = 0;
    tvhtrace(LS_EN50494, "using master's existing FD %d for group %d",
             group->lug_master_fd, group->lug_group_id);
  } else {
    /* Open FD ourselves */
    group->lug_master_fd = tvh_open(lfe->lfe_fe_path, O_RDWR | O_NONBLOCK, 0);
    if (group->lug_master_fd > 0) {
      group->lug_fd_owned = 1;
      tvhtrace(LS_EN50494, "opened master FD %d for group %d",
               group->lug_master_fd, group->lug_group_id);
    } else {
      tvherror(LS_EN50494, "failed to open master frontend %s for group %d",
               lfe->lfe_fe_path, group->lug_group_id);
    }
  }

  return group->lug_master_fd;
}

/*
 * Release master frontend FD
 * Must be called WITH group->lug_lock held
 */
static void
linuxdvb_unicable_release_master_fd ( linuxdvb_unicable_group_t *group )
{
  /* Only close if we opened it ourselves */
  if (group->lug_fd_owned && group->lug_master_fd > 0) {
    close(group->lug_master_fd);
    tvhtrace(LS_EN50494, "closed master FD for group %d", group->lug_group_id);
    group->lug_master_fd = -1;
    group->lug_fd_owned = 0;
  } else if (!group->lug_fd_owned) {
    /* FD belongs to master frontend, just clear our reference */
    group->lug_master_fd = -1;
  }
}

/*
 * Invalidate cached master for a group (called when master element is destroyed)
 */
void
linuxdvb_unicable_invalidate_master ( linuxdvb_satconf_ele_t *lse )
{
  linuxdvb_unicable_group_t *group;

  if (!lse || !lse->lse_en50494)
    return;

  linuxdvb_en50494_t *le = (linuxdvb_en50494_t *)lse->lse_en50494;
  if (le->le_group_id == 0 || !le->le_is_master)
    return;

  tvh_mutex_lock(&linuxdvb_unicable_groups_lock);
  TAILQ_FOREACH(group, &linuxdvb_unicable_groups, lug_link) {
    if (group->lug_group_id == le->le_group_id && group->lug_master_ele == lse) {
      tvh_mutex_lock(&group->lug_lock);
      linuxdvb_unicable_release_master_fd(group);
      group->lug_master_ele = NULL;
      tvh_mutex_unlock(&group->lug_lock);
      tvhtrace(LS_EN50494, "invalidated master for group %d", le->le_group_id);
      break;
    }
  }
  tvh_mutex_unlock(&linuxdvb_unicable_groups_lock);
}

static void
linuxdvb_en50494_class_get_title
  ( idnode_t *o, const char *lang, char *dst, size_t dstsize )
{
  const char *title = N_("Unicable I (EN50494)");
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, title));
}

static void
linuxdvb_en50607_class_get_title
  ( idnode_t *o, const char *lang, char *dst, size_t dstsize )
{
  const char *title = N_("Unicable II (EN50607)");
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, title));
}

static htsmsg_t *
linuxdvb_en50494_position_list ( void *o, const char *lang )
{
  uint32_t i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < 2; i++) {
    htsmsg_add_u32(m, NULL, i);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50494_id_list ( void *o, const char *lang )
{
  uint32_t i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < 8; i++) {
    htsmsg_add_u32(m, NULL, i);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50607_id_list ( void *o, const char *lang )
{
  uint32_t i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < 32; i++) {
    htsmsg_add_u32(m, NULL, i);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50494_pin_list ( void *o, const char *lang )
{
  int32_t i;

  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e;

  e = htsmsg_create_map();
  htsmsg_add_u32(e, "key", 256);
  htsmsg_add_str(e, "val", tvh_gettext_lang(lang, N_("No PIN")));
  htsmsg_add_msg(m, NULL, e);

  for (i = 0; i < 256; i++) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_u32(e, "val", i);
    htsmsg_add_msg(m, NULL, e);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50494_group_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e;
  int i;

  e = htsmsg_create_map();
  htsmsg_add_u32(e, "key", 0);
  htsmsg_add_str(e, "val", tvh_gettext_lang(lang, N_("None (standalone)")));
  htsmsg_add_msg(m, NULL, e);

  for (i = 1; i <= 15; i++) {
    char buf[32];
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    snprintf(buf, sizeof(buf), "%s %d", tvh_gettext_lang(lang, N_("Group")), i);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(m, NULL, e);
  }
  return m;
}

extern const idclass_t linuxdvb_diseqc_class;

const idclass_t linuxdvb_en50494_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_en50494",
  .ic_caption     = N_("en50494"),
  .ic_get_title   = linuxdvb_en50494_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type    = PT_U32,
      .id      = "powerup_time",
      .name    = N_("Power up time (ms) (10-500)"),
      .desc    = N_("Time (in milliseconds) for the Unicable device to power up."),
      .off     = offsetof(linuxdvb_en50494_t, le_powerup_time),
      .def.u32 = 15,
    },
    {
      .type    = PT_U32,
      .id      = "cmd_time",
      .name    = N_("Command time (ms) (10-300)"),
      .desc    = N_("Time (in milliseconds) for a command to complete."),
      .off     = offsetof(linuxdvb_en50494_t, le_cmd_time),
      .def.u32 = 50
    },
    {
      .type   = PT_U16,
      .id     = "id",
      .name   = N_("SCR (ID)"),
      .desc   = N_("SCR (Satellite Channel Router) ID."),
      .off    = offsetof(linuxdvb_en50494_t, le_id),
      .list   = linuxdvb_en50494_id_list,
    },
    {
      .type   = PT_U16,
      .id     = "frequency",
      .name   = N_("Frequency (MHz)"),
      .desc   = N_("User Band Frequency (in MHz)."),
      .off    = offsetof(linuxdvb_en50494_t, le_frequency),
    },
    {
      .type   = PT_U16,
      .id     = "pin",
      .name   = N_("PIN"),
      .desc   = N_("PIN."),
      .off    = offsetof(linuxdvb_en50494_t, le_pin),
      .list   = linuxdvb_en50494_pin_list,
    },
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = N_("Position"),
      .desc   = N_("Position ID."),
      .off    = offsetof(linuxdvb_en50494_t, le_position),
      .list   = linuxdvb_en50494_position_list,
    },
    {
      .type   = PT_U16,
      .id     = "unicable_group",
      .name   = N_("Unicable group"),
      .desc   = N_("Group ID for master/slave coordination. All unicable elements "
                   "in the same group share the same physical cable. The master "
                   "sends DiSEqC commands for all slaves in the group."),
      .off    = offsetof(linuxdvb_en50494_t, le_group_id),
      .list   = linuxdvb_en50494_group_list,
      .opts   = PO_ADVANCED,
    },
    {
      .type   = PT_BOOL,
      .id     = "is_master",
      .name   = N_("Unicable master"),
      .desc   = N_("If enabled, this unicable element acts as the master for its "
                   "group. The master's frontend sends DiSEqC commands for all "
                   "group members. Only one master per group is allowed."),
      .off    = offsetof(linuxdvb_en50494_t, le_is_master),
      .opts   = PO_ADVANCED,
    },
    {}
  }
};

const idclass_t linuxdvb_en50607_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_en50607",
  .ic_caption     = N_("en50607"),
  .ic_get_title   = linuxdvb_en50607_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type    = PT_U32,
      .id      = "powerup_time",
      .name    = N_("Power up time (ms) (10-500)"),
      .desc    = N_("Time (in milliseconds) for the Unicable device to power up."),
      .off     = offsetof(linuxdvb_en50494_t, le_powerup_time),
      .def.u32 = 15,
    },
    {
      .type    = PT_U32,
      .id      = "cmd_time",
      .name    = N_("Command time (ms) (10-300)"),
      .desc    = N_("Time (in milliseconds) for a command to complete."),
      .off     = offsetof(linuxdvb_en50494_t, le_cmd_time),
      .def.u32 = 50
    },
    {
      .type   = PT_U16,
      .id     = "id",
      .name   = N_("SCR (ID)"),
      .desc   = N_("SCR (Satellite Channel Router) ID."),
      .off    = offsetof(linuxdvb_en50494_t, le_id),
      .list   = linuxdvb_en50607_id_list,
    },
    {
      .type   = PT_U16,
      .id     = "frequency",
      .name   = N_("Frequency (MHz)"),
      .desc   = N_("User Band Frequency (in MHz)."),
      .off    = offsetof(linuxdvb_en50494_t, le_frequency),
    },
    {
      .type   = PT_U16,
      .id     = "pin",
      .name   = N_("PIN"),
      .desc   = N_("PIN."),
      .off    = offsetof(linuxdvb_en50494_t, le_pin),
      .list   = linuxdvb_en50494_pin_list,
    },
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = N_("Position"),
      .desc   = N_("Position ID."),
      .off    = offsetof(linuxdvb_en50494_t, le_position),
      .list   = linuxdvb_en50494_position_list,
    },
    {
      .type   = PT_U16,
      .id     = "unicable_group",
      .name   = N_("Unicable group"),
      .desc   = N_("Group ID for master/slave coordination (0=standalone). "
                   "All unicable elements in the same group share DiSEqC "
                   "command serialization through the designated master."),
      .off    = offsetof(linuxdvb_en50494_t, le_group_id),
      .list   = linuxdvb_en50494_group_list,
      .opts   = PO_ADVANCED,
    },
    {
      .type   = PT_BOOL,
      .id     = "is_master",
      .name   = N_("Unicable master"),
      .desc   = N_("If enabled, this unicable element acts as the master for its "
                   "group. The master's frontend sends DiSEqC commands for all "
                   "group members. Only one master per group is allowed."),
      .off    = offsetof(linuxdvb_en50494_t, le_is_master),
      .opts   = PO_ADVANCED,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_en50494_freq0
  ( linuxdvb_en50494_t *le, int freq, int *rfreq, uint16_t *t )
{
  /* transponder value - t */
  *t = round((((freq / 1000) + 2 + le->le_frequency) / 4) - 350);
  if (*t >= 1023) {
    tvherror(LS_EN50494, "transponder value bigger than 1023 for freq %d (%d)", freq, le->le_frequency);
    return -1;
  }

  /* tune frequency for the frontend */
  *rfreq = (*t + 350) * 4000 - freq;
  return 0;
}

static int
linuxdvb_en50607_freq0
  ( linuxdvb_en50494_t *le, int freq, int *rfreq, uint16_t *t )
{
  /* transponder value - t */
  *t = round((double)freq / 1000) - 100;
  if (*t > 2047) {
    tvherror(LS_EN50494, "transponder value bigger than 2047 for freq %d (%d)", freq, le->le_frequency);
    return -1;
  }

  /* tune frequency for the frontend */
  *rfreq = le->le_frequency * 1000;
  return 0;
}

static int
linuxdvb_en50494_freq
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm, int freq )
{
  linuxdvb_en50494_t *le = (linuxdvb_en50494_t*) ld;
  int rfreq;
  uint16_t t;

  if (linuxdvb_en50494_freq0(le, freq, &rfreq, &t))
    return -1;
  return rfreq;
}

static int
linuxdvb_en50494_match
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm1, dvb_mux_t *lm2 )
{
  return lm1 == lm2;
}

/*
 * Internal tune function - sends unicable command using specified FD and satconf
 * Parameters:
 *   fd       - frontend FD to use for DiSEqC commands
 *   volt_lsp - satconf to use for voltage control (master's satconf for slaves)
 *   repeats  - number of DiSEqC repeats
 */
static int
linuxdvb_en50494_tune_internal
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
    linuxdvb_satconf_ele_t *sc,
    int vol, int pol, int band, int freq,
    int fd, linuxdvb_satconf_t *volt_lsp, int repeats )
{
  int ret = 0, i, rfreq;
  int ver2 = linuxdvb_unicable_is_en50607(ld->ld_type);
  linuxdvb_en50494_t *le = (linuxdvb_en50494_t*) ld;
  uint8_t data1, data2, data3;
  uint16_t t;

  if (!ver2) {
    /* tune frequency for the frontend */
    if (linuxdvb_en50494_freq0(le, freq, &rfreq, &t))
      return -1;
    le->le_tune_freq = rfreq;
    /* 2 data fields (16bit) */
    data1  = (le->le_id & 7) << 5;        /* 3bit user-band */
    data1 |= (le->le_position & 1) << 4;  /* 1bit position (satellite A(0)/B(1)) */
    data1 |= (pol & 1) << 3;              /* 1bit polarization v(0)/h(1) */
    data1 |= (band & 1) << 2;             /* 1bit band lower(0)/upper(1) */
    data1 |= (t >> 8) & 3;                /* 2bit transponder value bit 1-2 */
    data2  = t & 0xff;                    /* 8bit transponder value bit 3-10 */
    data3  = 0;
  } else {
    /* tune frequency for the frontend */
    if (linuxdvb_en50607_freq0(le, freq, &rfreq, &t))
      return -1;
    le->le_tune_freq = rfreq;
    /* 3 data fields (24bit) */
    data1  = (le->le_id & 0x1f) << 3;     /* 5bit user-band */
    data1 |= (t >> 8) & 7;                /* 3bit transponder value bit 1-3 */
    data2  = t & 0xff;                    /* 8bit transponder value bit 4-11 */
    data3  = (le->le_position & 0x3f) << 2; /* 6bit position */
    data3 |= (pol & 1) << 1;              /* 1bit polarization v(0)/h(1) */
    data3 |= band & 1;                    /* 1bit band lower(0)/upper(1) */
  }

  /* setup en50494 switch */
  for (i = 0; i <= repeats; i++) {
    /* to avoid repeated collision, wait a random time 68-118
     * 67,5 is the typical diseqc-time */
    if (i != 0) {
      uint8_t rnd;
      uuid_random(&rnd, 1);
      int ms = ((int)rnd)%50 + 68;
      tvh_safe_usleep(ms*1000);
    }

    /* use 18V */
    ret = linuxdvb_diseqc_set_volt(volt_lsp, 1);
    if (ret) {
      tvherror(LS_EN50494, "error setting lnb voltage to 18V");
      break;
    }

    /* linuxdvb_diseqc_set_volt() function already sleeps for 15ms */
    tvhtrace(LS_EN50494, "after power up: sleep %d ms", le->le_powerup_time);
    tvh_safe_usleep(MINMAX(le->le_powerup_time, 10, 500) * 1000); /* standard: 4ms < x < 22ms */

    /* send tune command (with/without pin) */
    tvhdebug(LS_EN50494,
             "lnb=%i id=%i freq=%i pin=%i v/h=%i l/u=%i f=%i, data=0x%02X%02X%02X",
             le->le_position, le->le_id, le->le_frequency, le->le_pin, pol,
             band, freq, data1, data2, data3);
    if (!ver2 && le->le_pin != LINUXDVB_EN50494_NOPIN) {
      ret = linuxdvb_diseqc_send(fd,
                                 LINUXDVB_EN50494_FRAME,
                                 LINUXDVB_EN50494_ADDRESS,
                                 LINUXDVB_EN50494_CMD_NORMAL_MULTIHOME,
                                 3,
                                 data1, data2, (uint8_t)le->le_pin);
    } else if (!ver2) {
      ret = linuxdvb_diseqc_send(fd,
                                 LINUXDVB_EN50494_FRAME,
                                 LINUXDVB_EN50494_ADDRESS,
                                 LINUXDVB_EN50494_CMD_NORMAL,
                                 2,
                                 data1, data2);
    } else if (ver2 && le->le_pin != LINUXDVB_EN50494_NOPIN) {
      ret = linuxdvb_diseqc_raw_send(fd, 5,
                                     LINUXDVB_EN50607_FRAME_MULTIHOME,
                                     data1, data2, data3, (uint8_t)le->le_pin);
    } else if (ver2) {
      ret = linuxdvb_diseqc_raw_send(fd, 4,
                                     LINUXDVB_EN50607_FRAME_NORMAL,
                                     data1, data2, data3);
    }
    if (ret != 0) {
      tvherror(LS_EN50494, "error send tune command");
      break;
    }

    tvhtrace(LS_EN50494, "after command: sleep %d ms", le->le_cmd_time);
    tvh_safe_usleep(MINMAX(le->le_cmd_time, 10, 300) * 1000); /* standard: 2ms < x < 60ms */

    /* return to 13V */
    ret = linuxdvb_diseqc_set_volt(volt_lsp, 0);
    if (ret) {
      tvherror(LS_EN50494, "error setting lnb voltage back to 13V");
      break;
    }
  }

  return ret == 0 ? 0 : -1;
}

/*
 * Public tune function - routes through master if this is a slave element
 */
static int
linuxdvb_en50494_tune
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *sc,
    int vol, int pol, int band, int freq )
{
  linuxdvb_en50494_t *le = (linuxdvb_en50494_t*) ld;
  linuxdvb_unicable_group_t *group = NULL;
  linuxdvb_satconf_t *volt_lsp;
  int fd, ret;
  int repeats = sc->lse_parent->ls_diseqc_repeats;

  /*
   * Check if this is a slave element that should route through master
   */
  if (le->le_group_id > 0 && !le->le_is_master) {
    /* Slave element - route through master */
    group = linuxdvb_unicable_group_get(le->le_group_id);
    if (!group) {
      tvherror(LS_EN50494, "failed to get unicable group %d", le->le_group_id);
      return -1;
    }

    /* Acquire per-group lock */
    tvh_mutex_lock(&group->lug_lock);

    /* Get master's FD */
    fd = linuxdvb_unicable_get_master_fd(group);
    if (fd < 0) {
      tvherror(LS_EN50494, "failed to get master FD for group %d", le->le_group_id);
      tvh_mutex_unlock(&group->lug_lock);
      return -1;
    }

    /* Use master's satconf for voltage control */
    volt_lsp = group->lug_master_ele->lse_parent;

    tvhtrace(LS_EN50494, "slave routing through master: group=%d fd=%d",
             le->le_group_id, fd);

    /* Execute tune via master */
    ret = linuxdvb_en50494_tune_internal(ld, lm, sc, vol, pol, band, freq,
                                         fd, volt_lsp, repeats);

    /* Release master FD and group lock */
    linuxdvb_unicable_release_master_fd(group);
    tvh_mutex_unlock(&group->lug_lock);

    return ret;
  }

  /*
   * Master or standalone element - use own FD with global lock
   */
  fd = linuxdvb_satconf_fe_fd(lsp);
  volt_lsp = lsp;

  /* wait until no other thread is setting up switch.
   * when an other thread was blocking, waiting 20ms.
   */
  if (tvh_mutex_trylock(&linuxdvb_en50494_lock) != 0) {
    if (tvh_mutex_lock(&linuxdvb_en50494_lock) != 0) {
      tvherror(LS_EN50494,"failed to lock for tuning");
      return -1;
    }
    tvh_safe_usleep(20000);
  }

  ret = linuxdvb_en50494_tune_internal(ld, lm, sc, vol, pol, band, freq,
                                       fd, volt_lsp, repeats);

  tvh_mutex_unlock(&linuxdvb_en50494_lock);

  return ret;
}


/* **************************************************************************
 * Create / Config
 * *************************************************************************/

void
linuxdvb_en50494_init (void)
{
  if (tvh_mutex_init(&linuxdvb_en50494_lock, NULL) != 0) {
    tvherror(LS_EN50494, "failed to init lock mutex");
  }
  TAILQ_INIT(&linuxdvb_unicable_groups);
  if (tvh_mutex_init(&linuxdvb_unicable_groups_lock, NULL) != 0) {
    tvherror(LS_EN50494, "failed to init groups lock mutex");
  }
}

void
linuxdvb_en50494_done (void)
{
  linuxdvb_unicable_group_t *group;

  tvh_mutex_lock(&linuxdvb_unicable_groups_lock);
  while ((group = TAILQ_FIRST(&linuxdvb_unicable_groups)) != NULL) {
    TAILQ_REMOVE(&linuxdvb_unicable_groups, group, lug_link);
    if (group->lug_fd_owned && group->lug_master_fd >= 0)
      close(group->lug_master_fd);
    tvh_mutex_destroy(&group->lug_lock);
    free(group);
  }
  tvh_mutex_unlock(&linuxdvb_unicable_groups_lock);
  tvh_mutex_destroy(&linuxdvb_unicable_groups_lock);
  tvh_mutex_destroy(&linuxdvb_en50494_lock);
}

htsmsg_t *
linuxdvb_en50494_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_msg(m, NULL, htsmsg_create_key_val("", tvh_gettext_lang(lang, N_("None"))));
  htsmsg_add_msg(m, NULL, htsmsg_create_key_val(UNICABLE_I_NAME, tvh_gettext_lang(lang, N_(UNICABLE_I_NAME))));
  htsmsg_add_msg(m, NULL, htsmsg_create_key_val(UNICABLE_II_NAME, tvh_gettext_lang(lang, N_(UNICABLE_II_NAME))));
  return m;
}

linuxdvb_diseqc_t *
linuxdvb_en50494_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int port )
{
  linuxdvb_diseqc_t *ld;
  linuxdvb_en50494_t *le;

  if (strcmp(name ?: "", "Generic") &&
      strcmp(name ?: "", UNICABLE_I_NAME) &&
      strcmp(name ?: "", UNICABLE_II_NAME))
    return NULL;

  if (linuxdvb_unicable_is_en50494(name) && port > 1) {
    tvherror(LS_EN50494, "only 2 ports/positions are possible. given %i", port);
    port = 0;
  }

  if (linuxdvb_unicable_is_en50607(name) && port > 63) {
    tvherror(LS_EN50494, "only 64 ports/positions are possible. given %i", port);
    port = 0;
  }

  le = calloc(1, sizeof(linuxdvb_en50494_t));
  if (le == NULL)
    return NULL;
  le->le_position  = port;
  le->le_id        = 0;
  le->le_frequency = 0;
  le->le_pin       = LINUXDVB_EN50494_NOPIN;
  le->ld_freq      = linuxdvb_en50494_freq;
  le->ld_match     = linuxdvb_en50494_match;
  if (le->le_powerup_time == 0)
      le->le_powerup_time = 15;
  if (le->le_cmd_time == 0)
      le->le_cmd_time = 50;

  ld = linuxdvb_diseqc_create0((linuxdvb_diseqc_t *)le,
                               NULL,
                               linuxdvb_unicable_is_en50607(name) ?
                                 &linuxdvb_en50607_class :
                                 &linuxdvb_en50494_class,
                               conf, name, ls);
  if (ld) {
    ld->ld_tune = linuxdvb_en50494_tune;
    /* May not needed: ld->ld_grace = linuxdvb_en50494_grace; */
  }

  return ld;
}

void
linuxdvb_en50494_destroy ( linuxdvb_diseqc_t *le )
{
  linuxdvb_diseqc_destroy(le);
  free(le);
}
