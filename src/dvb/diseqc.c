#include <time.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>

#include "tvheadend.h"
#include "diseqc.h"

/*--------------------------------------------------------------------------*/

static inline void
msleep(uint32_t msec)
{
  struct timespec req = { msec / 1000, 1000000 * (msec % 1000) };

  while (nanosleep(&req, &req))
    ;
}

int
diseqc_send_msg(int fe_fd, __u8 framing_byte, __u8 address, __u8 cmd,
                    __u8 data_1,  __u8 data_2, __u8 data_3, __u8 msg_len)
{
  struct dvb_diseqc_master_cmd message;

#if 0
  tvhlog(LOG_INFO, "diseqc", "sending %X %X %X %X %X %X",
         framing_byte, address, cmd, data_1, data_2, data_3);
#endif
  
  message.msg[0] = framing_byte;
  message.msg[1] = address;
  message.msg[2] = cmd;
  message.msg[3] = data_1;
  message.msg[4] = data_2;
  message.msg[5] = data_3;
  message.msg_len = msg_len;
  return ioctl(fe_fd, FE_DISEQC_SEND_MASTER_CMD, &message);
}

int
diseqc_setup(int fe_fd, int lnb_num, int voltage, int band,
              uint32_t version, uint32_t repeats)
{
  int i = (lnb_num % 4) * 4 + voltage * 2 + (band ? 1 : 0);
  int j = lnb_num / 4;
  int err;

#if 0
  tvhlog(LOG_INFO, "diseqc",
        "fe_fd %i, lnb_num %i, voltage %i, band %i, version %i, repeats %i",
        fe_fd, lnb_num, voltage, band, version, repeats);
#endif

  /* verify lnb number and diseqc data */
  if(lnb_num < 0 || lnb_num >=16 || i < 0 || i >= 16 || j < 0 || j >= 4)
    return -1;

  /* turn off continuous tone */
  if ((err = ioctl(fe_fd, FE_SET_TONE, SEC_TONE_OFF)))
    return err;

  /* set lnb voltage */
  if ((err = ioctl(fe_fd, FE_SET_VOLTAGE,
                    (i/2) % 2 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13)))
    return err;
  msleep(15);

  switch (repeats) {
    case 0:     /* send uncommited msg, wait 15ms, send commited msg */
      if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x39, 0xF0 | j, 0, 0, 4)))
        return err;
      msleep(15);
      if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
        return err;
      break;

    default:     /* commited msg, 25ms, uncommited msg, 25ms, commited msg */
      /* send commited message */
      if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
        return err;
      msleep(25);
      if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x39, 0xF0 | j, 0, 0, 4)))
        return err;
      msleep(25);
      if ((err = diseqc_send_msg(fe_fd, 0xE1, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
        return err;
      if (repeats == 1)         /* if 2 repeats, do it again */
        break;

      msleep(25);
      if ((err = diseqc_send_msg(fe_fd, 0xE1, 0x10, 0x39, 0xF0 | j, 0, 0, 4)))
        return err;
      msleep(25);
      if ((err = diseqc_send_msg(fe_fd, 0xE1, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
        return err;
  }
  msleep(15);

  /* set toneburst */
  if ((err = ioctl(fe_fd, FE_DISEQC_SEND_BURST,
                    (i/4) % 2 ? SEC_MINI_B : SEC_MINI_A)))
    return err;
  msleep(15);

  /* set continuous tone */
  if ((err = ioctl(fe_fd, FE_SET_TONE, i % 2 ? SEC_TONE_ON : SEC_TONE_OFF)))
    return err;
  return 0;
}

int
diseqc_voltage_off(int fe_fd)
{
  return ioctl(fe_fd, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);
}
