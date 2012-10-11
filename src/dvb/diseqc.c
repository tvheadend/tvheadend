#include <time.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>

#include "tvheadend.h"
#include "diseqc.h"

//#define DISEQC_TRACE

struct dvb_diseqc_master_cmd diseqc_commited_cmds[] = {
  { { 0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf1, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf2, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf3, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf4, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf5, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf6, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf7, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf8, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xf9, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xfa, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xfb, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xfc, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xfd, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xfe, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x38, 0xff, 0x00, 0x00 }, 4 }
};

struct dvb_diseqc_master_cmd diseqc_uncommited_cmds[] = {
  { { 0xe0, 0x10, 0x39, 0xf0, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x39, 0xf1, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x39, 0xf2, 0x00, 0x00 }, 4 },
  { { 0xe0, 0x10, 0x39, 0xf3, 0x00, 0x00 }, 4 }
};

/*--------------------------------------------------------------------------*/

static inline void
msleep(uint32_t msec)
{
  struct timespec req = { msec / 1000, 1000000 * (msec % 1000) };

  while (nanosleep(&req, &req))
    ;
}

int
diseqc_setup(int fe_fd, int input, int voltage, int band, int diseqc_ver)
{
  int i = (input % 4) * 4 + voltage * 2 + (band ? 1 : 0);
  int j = input / 4;
  int err;

#ifdef DISEQC_TRACE
  tvhlog(LOG_INFO, "diseqc",
        "fe_fd %i, input %i, voltage %i, band %i, diseqc_ver %i, i %i, j %i",
        fe_fd, input, voltage, band, diseqc_ver, i, j);
#endif
  /* check for invalid input number or diseqc command indexes */
  if(input < 0 || input >=16 || i < 0 || i >= 16 || j < 0 || j >= 4)
    return -1;

  /* turn off continuous tone */
  if ((err = ioctl(fe_fd, FE_SET_TONE, SEC_TONE_OFF)))
    return err;

  /* set lnb voltage */
  if ((err = ioctl(fe_fd, FE_SET_VOLTAGE,
                    (i/2) % 2 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13)))
    return err;
  msleep(15);

  /* send uncommited command */
  if ((err = ioctl(fe_fd, FE_DISEQC_SEND_MASTER_CMD,
                    &diseqc_uncommited_cmds[j])))
    return err;
  msleep(15);

  /* send commited command */
  if ((err = ioctl(fe_fd, FE_DISEQC_SEND_MASTER_CMD,
                    &diseqc_commited_cmds[i])))
    return err;
#ifdef DISEQC_TRACE
  tvhlog(LOG_INFO, "diseqc", "E0 10 39 F%X - E0 10 38 F%X sent", j, i);
#endif
  msleep(15);

  /* send toneburst command */
  if ((err = ioctl(fe_fd, FE_DISEQC_SEND_BURST,
                    (i/4) % 2 ? SEC_MINI_B : SEC_MINI_A)))
    return err;
  msleep(15);

  /* set continuous tone */
  if ((ioctl(fe_fd, FE_SET_TONE, i % 2 ? SEC_TONE_ON : SEC_TONE_OFF)))
    return err;
  return 0;
}

int
diseqc_voltage_off(int fe_fd)
{
  return ioctl(fe_fd, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);
}
