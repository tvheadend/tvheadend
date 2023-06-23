/*
 *  Tvheadend - NetCeiver DVB input
 *
 *  Copyright (C) 2013-2018 Sven Wegener
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
#include "netceiver_private.h"
#include "notify.h"
#include "atomic.h"
#include "tvhpoll.h"
#include "settings.h"
#include "input.h"
#include "udp.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>


static void mcg_htons(struct in6_addr *mcg)
{
  int i;

  for (i = 0; i < 8; i++) {
    mcg->s6_addr16[i] = htons(mcg->s6_addr16[i]);
  }
}

static void __attribute__((__unused__)) mcg_ntohs(struct in6_addr *mcg)
{
  int i;

  for (i = 0; i < 8; i++) {
    mcg->s6_addr16[i] = ntohs(mcg->s6_addr16[i]);
  }
}

typedef struct netceiver_tune_params {
  unsigned int prefix:16;
  unsigned int group:4;
  unsigned int priority:4;
  unsigned int receptionsystem:8;

  unsigned int camhandling:16;
  unsigned int polarisation:4;
  unsigned int satposition:12;

  unsigned int symbolrate:16;
  unsigned int modulation:14;
  unsigned int inversion:2;

  unsigned int frequency:19;
  unsigned int pid:13;
} netceiver_tune_params_t;

typedef struct netceiver_tbl {
  int t;
  int n;
} netceiver_tbl_t;

#define TABLE_END_DEFAULT -1

static int tvh2netceiver(const netceiver_tbl_t *tbl, int key)
{
  while (tbl->t != TABLE_END_DEFAULT) {
    if (tbl->t == key)
      return tbl->n;
    tbl++;
  }
  return tbl->n;
}

static const netceiver_tbl_t netceiver_polarisation_tbl[] = {
  { .t = DVB_POLARISATION_HORIZONTAL, .n = 1, },
  { .t = DVB_POLARISATION_VERTICAL,   .n = 0, },
  { .t = TABLE_END_DEFAULT,           .n = 2, },
};

static const netceiver_tbl_t netceiver_modulation_tbl[] = {
  { .t = DVB_MOD_QAM_16,    .n = 1, },
  { .t = DVB_MOD_QAM_32,    .n = 2, },
  { .t = DVB_MOD_QAM_64,    .n = 3, },
  { .t = DVB_MOD_QAM_128,   .n = 4, },
  { .t = DVB_MOD_QAM_256,   .n = 5, },
  { .t = DVB_MOD_QAM_AUTO,  .n = 6, },
  { .t = TABLE_END_DEFAULT, .n = 6, },
};

static const netceiver_tbl_t netceiver_dvbs2_modulation_tbl[] = {
  { .t = DVB_MOD_QPSK,      .n = 1, },
  { .t = DVB_MOD_PSK_8,     .n = 2, },
  { .t = TABLE_END_DEFAULT, .n = 0, },
};

static const netceiver_tbl_t netceiver_transmission_mode_tbl[] = {
  { .t = DVB_TRANSMISSION_MODE_AUTO,  .n = 2, },
  { .t = DVB_TRANSMISSION_MODE_1K,    .n = 4, },
  { .t = DVB_TRANSMISSION_MODE_2K,    .n = 0, },
  { .t = DVB_TRANSMISSION_MODE_4K,    .n = 3, },
  { .t = DVB_TRANSMISSION_MODE_8K,    .n = 1, },
  { .t = DVB_TRANSMISSION_MODE_16K,   .n = 5, },
  { .t = DVB_TRANSMISSION_MODE_32K,   .n = 6, },
  { .t = DVB_TRANSMISSION_MODE_C1,    .n = 7, },
  { .t = DVB_TRANSMISSION_MODE_C3780, .n = 8, },
  { .t = TABLE_END_DEFAULT,           .n = 2, },
};

static const netceiver_tbl_t netceiver_fec_tbl[] = {
  { .t = DVB_FEC_NONE,      .n = 0, },
  { .t = DVB_FEC_AUTO,      .n = 9, },
  { .t = DVB_FEC_1_2,       .n = 1, },
  { .t = DVB_FEC_2_3,       .n = 2, },
  { .t = DVB_FEC_2_5,       .n = 12, },
  { .t = DVB_FEC_3_4,       .n = 3, },
  { .t = DVB_FEC_3_5,       .n = 10, },
  { .t = DVB_FEC_4_5,       .n = 4, },
  { .t = DVB_FEC_5_6,       .n = 5, },
  { .t = DVB_FEC_6_7,       .n = 6, },
  { .t = DVB_FEC_7_8,       .n = 7, },
  { .t = DVB_FEC_8_9,       .n = 8, },
  { .t = DVB_FEC_9_10,      .n = 11, },
  { .t = TABLE_END_DEFAULT, .n = 9, },
};

static const netceiver_tbl_t netceiver_inversion_tbl[] = {
  { .t = DVB_INVERSION_OFF,   .n = 0, },
  { .t = DVB_INVERSION_ON,    .n = 1, },
  { .t = DVB_INVERSION_AUTO,  .n = 2, },
  { .t = TABLE_END_DEFAULT,   .n = 2, },
};

static const netceiver_tbl_t netceiver_bandwidth_tbl[] = {
  { .t = DVB_BANDWIDTH_8_MHZ,     .n = 0, },
  { .t = DVB_BANDWIDTH_7_MHZ,     .n = 1, },
  { .t = DVB_BANDWIDTH_6_MHZ,     .n = 2, },
  { .t = DVB_BANDWIDTH_AUTO,      .n = 3, },
  { .t = DVB_BANDWIDTH_5_MHZ,     .n = 4, },
  { .t = DVB_BANDWIDTH_10_MHZ,    .n = 5, },
  { .t = DVB_BANDWIDTH_1_712_MHZ, .n = 6, },
  { .t = TABLE_END_DEFAULT,       .n = 3, },
};

static const netceiver_tbl_t netceiver_guard_interval_tbl[] = {
  { .t = DVB_GUARD_INTERVAL_1_32,   .n = 0, },
  { .t = DVB_GUARD_INTERVAL_1_16,   .n = 1, },
  { .t = DVB_GUARD_INTERVAL_1_8,    .n = 2, },
  { .t = DVB_GUARD_INTERVAL_1_4,    .n = 3, },
  { .t = DVB_GUARD_INTERVAL_AUTO,   .n = 4, },
  { .t = DVB_GUARD_INTERVAL_1_128,  .n = 5, },
  { .t = DVB_GUARD_INTERVAL_19_128, .n = 6, },
  { .t = DVB_GUARD_INTERVAL_19_256, .n = 7, },
  { .t = DVB_GUARD_INTERVAL_PN420,  .n = 8, },
  { .t = DVB_GUARD_INTERVAL_PN595,  .n = 9, },
  { .t = DVB_GUARD_INTERVAL_PN945,  .n = 10, },
  { .t = TABLE_END_DEFAULT,         .n = 4, },
};

static const netceiver_tbl_t netceiver_hierarchy_tbl[] = {
  { .t = DVB_HIERARCHY_NONE,  .n = 0, },
  { .t = DVB_HIERARCHY_1,     .n = 1, },
  { .t = DVB_HIERARCHY_2,     .n = 2, },
  { .t = DVB_HIERARCHY_4,     .n = 3, },
  { .t = DVB_HIERARCHY_AUTO,  .n = 4, },
  { .t = TABLE_END_DEFAULT,   .n = 4, },
};

static void dm2tune_general(netceiver_tune_params_t *tune, dvb_mux_t *dm)
{
  tune->satposition = -1;
  tune->inversion = tvh2netceiver(netceiver_inversion_tbl, dm->lm_tuning.dmc_fe_inversion);
}

static void dm2tune_dvbs(netceiver_tune_params_t *tune, dvb_mux_t *dm)
{
  tune->receptionsystem = 0;
  tune->satposition = 1800 + dvb_network_get_orbital_pos(dm->mm_network);
  tune->frequency = (dm->lm_tuning.dmc_fe_freq + 24) / 50;
  tune->symbolrate = dm->lm_tuning.u.dmc_fe_qpsk.symbol_rate / 1000;
  tune->modulation = tvh2netceiver(netceiver_fec_tbl, dm->lm_tuning.u.dmc_fe_qpsk.fec_inner);
  tune->polarisation = tvh2netceiver(netceiver_polarisation_tbl, dm->lm_tuning.u.dmc_fe_qpsk.polarisation);

  if (dm->lm_tuning.dmc_fe_delsys == DVB_SYS_DVBS2) {
    /* the netceiver treats DVB-S2 as a separate frontend type */
    tune->receptionsystem = 4;
    tune->modulation |= tvh2netceiver(netceiver_dvbs2_modulation_tbl, dm->lm_tuning.dmc_fe_modulation) << 4;
  }
}

static void dm2tune_dvbc(netceiver_tune_params_t *tune, dvb_mux_t *dm)
{
  tune->receptionsystem = 1;
  tune->frequency = ((unsigned long long) dm->lm_tuning.dmc_fe_freq + 1041) * 12 / 25000;
  tune->symbolrate = dm->lm_tuning.u.dmc_fe_qam.symbol_rate / 200;
  tune->modulation = tvh2netceiver(netceiver_modulation_tbl, dm->lm_tuning.dmc_fe_modulation);
}

static void dm2tune_dvbt(netceiver_tune_params_t *tune, dvb_mux_t *dm)
{
  tune->receptionsystem = 2;
  tune->frequency = ((unsigned long long) dm->lm_tuning.dmc_fe_freq + 1041) * 12 / 25000;
  tune->symbolrate = 0;
  tune->symbolrate |= tvh2netceiver(netceiver_transmission_mode_tbl, dm->lm_tuning.u.dmc_fe_ofdm.transmission_mode) << 8;
  tune->symbolrate |= tvh2netceiver(netceiver_fec_tbl, dm->lm_tuning.u.dmc_fe_ofdm.code_rate_HP) << 4;
  tune->symbolrate |= tvh2netceiver(netceiver_fec_tbl, dm->lm_tuning.u.dmc_fe_ofdm.code_rate_LP);
  tune->modulation = 0;
  tune->modulation |= tvh2netceiver(netceiver_guard_interval_tbl, dm->lm_tuning.u.dmc_fe_ofdm.guard_interval) << 9;
  tune->modulation |= tvh2netceiver(netceiver_bandwidth_tbl, dm->lm_tuning.u.dmc_fe_ofdm.bandwidth) << 7;
  tune->modulation |= tvh2netceiver(netceiver_hierarchy_tbl, dm->lm_tuning.u.dmc_fe_ofdm.hierarchy_information) << 4;
  tune->modulation |= tvh2netceiver(netceiver_modulation_tbl, dm->lm_tuning.dmc_fe_modulation);
}

static void dm2tune_atsc(netceiver_tune_params_t *tune, dvb_mux_t *dm)
{
  tvhwarn(LS_NETCEIVER, "ATSC is untested and probably incomplete");
  tune->receptionsystem = 3;
  tune->frequency = ((unsigned long long) dm->lm_tuning.dmc_fe_freq + 1041) * 12 / 25000;
}

static void dm2tune(netceiver_tune_params_t *tune, dvb_mux_t *dm)
{
  dm2tune_general(tune, dm);

  switch (dm->lm_tuning.dmc_fe_type) {
    case DVB_TYPE_S:
      dm2tune_dvbs(tune, dm);
      break;
    case DVB_TYPE_C:
      dm2tune_dvbc(tune, dm);
      break;
    case DVB_TYPE_T:
      dm2tune_dvbt(tune, dm);
      break;
    case DVB_TYPE_ATSC_T:
      dm2tune_atsc(tune, dm);
      break;
    default:
      tvherror(LS_NETCEIVER, "unsupported frontend type: %s", dvb_type2str(dm->lm_tuning.dmc_fe_type));
      break;
  }
}

static void tune2mcg(struct in6_addr *mcg, netceiver_tune_params_t *tune)
{
  mcg->s6_addr16[0] = tune->prefix;
  mcg->s6_addr16[1] = tune->group << 12 | tune->priority << 8 | tune->receptionsystem;
  mcg->s6_addr16[2] = tune->camhandling;
  mcg->s6_addr16[3] = tune->polarisation << 12 | tune->satposition;
  mcg->s6_addr16[4] = tune->symbolrate;
  mcg->s6_addr16[5] = tune->inversion << 14 | tune->modulation;
  mcg->s6_addr16[6] = tune->frequency;
  mcg->s6_addr16[7] = (tune->frequency >> 16) << 13 | tune->pid;

  mcg_htons(mcg);
}

static void dm2mcg(struct in6_addr *mcg, netceiver_group_t group, dvb_mux_t *dm, int priority, int pid, int sid)
{
  netceiver_tune_params_t tune;

  memset(&tune, 0, sizeof(tune));
  if (dm)
    dm2tune(&tune, dm);
  tune.prefix = NETCEIVER_MC_PREFIX;
  tune.group = group;
  tune.priority = priority;
  tune.pid = pid;
  tune.camhandling = sid;
  tune2mcg(mcg, &tune);
}

udp_connection_t *netceiver_tune(const char *name, const char *interface, netceiver_group_t group, int priority, dvb_mux_t *dm, int pid, int sid)
{
  char mcg_buf[INET6_ADDRSTRLEN];
  udp_connection_t *udp;
  struct in6_addr mcg;

  dm2mcg(&mcg, group, dm, priority, pid, sid);

  inet_ntop(AF_INET6, &mcg, mcg_buf, sizeof(mcg_buf));
  tvhdebug(LS_NETCEIVER, "%s - streaming %s on interface %s", name, mcg_buf, interface);

  udp = udp_bind(LS_NETCEIVER, name, mcg_buf, NETCEIVER_MC_PORT, NULL, interface, NETCEIVER_TS_IP_BUFFER_SIZE, 0);
  if (udp == UDP_FATAL_ERROR)
    udp = NULL;

  return udp;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
