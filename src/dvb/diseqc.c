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
  int err;
  struct dvb_diseqc_master_cmd message;

  tvhtrace("diseqc", "sending %X %X %X %X %X %X",
           framing_byte, address, cmd, data_1, data_2, data_3);
  
  message.msg[0] = framing_byte;
  message.msg[1] = address;
  message.msg[2] = cmd;
  message.msg[3] = data_1;
  message.msg[4] = data_2;
  message.msg[5] = data_3;
  message.msg_len = msg_len;
  if ((err = ioctl(fe_fd, FE_DISEQC_SEND_MASTER_CMD, &message))) {
	tvhlog(LOG_ERR, "diseqc", "error sending diseqc command");
    return err;
  }
  return 0;
}

int
diseqc_setup(int fe_fd, int lnb_num, int voltage, int band,
              uint32_t version, uint32_t repeats)
{
  int i = (lnb_num % 4) * 4 + voltage * 2 + (band ? 1 : 0);
  int j = lnb_num / 4;
  int k, err;

  tvhtrace("diseqc",
           "fe_fd=%i, lnb_num=%i, voltage=%i, band=%i, version=%i, repeats=%i",
           fe_fd, lnb_num, voltage, band, version, repeats);

  /* verify lnb number and diseqc data */
  if(lnb_num < 0 || lnb_num >=64 || i < 0 || i >= 16 || j < 0 || j >= 16)
    return -1;
  /* paranoia check: we must not come here in case of unicable... */
  if (version > 1) {
    tvhlog(LOG_ERR, "diseqc", "diseqc_setup called but version is %d", version);
    return -1;
  }

  /* turn off continuous tone */
  tvhtrace("diseqc", "disabling continuous tone");
  if ((err = ioctl(fe_fd, FE_SET_TONE, SEC_TONE_OFF))) {
	tvhlog(LOG_ERR, "diseqc", "error trying to turn off continuous tone");
    return err;
  }

  /* set lnb voltage */
  tvhtrace("diseqc", "setting lnb voltage to %iV", (i/2) % 2 ? 18 : 13);
  if ((err = ioctl(fe_fd, FE_SET_VOLTAGE, (i/2) % 2 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13))) {
	tvhlog(LOG_ERR, "diseqc", "error setting lnb voltage");
    return err;
  }
  msleep(15);

  if (repeats == 0) { /* uncommited msg, wait 15ms, commited msg */
    if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x39, 0xF0 | j, 0, 0, 4)))
      return err;
    msleep(15);
    if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
      return err;
  } else { /* commited msg, 25ms, uncommited msg, 25ms, commited msg, etc */
    if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
      return err;
    for (k = 0; k < repeats; k++) {
      msleep(25);
      if ((err = diseqc_send_msg(fe_fd, 0xE0, 0x10, 0x39, 0xF0 | j, 0, 0, 4)))
        return err;
      msleep(25);
      if ((err = diseqc_send_msg(fe_fd, 0xE1, 0x10, 0x38, 0xF0 | i, 0, 0, 4)))
        return err;
    }
  }
  msleep(15);

  /* set toneburst */
  tvhtrace("diseqc", (i/4) % 2 ? "sending mini diseqc B" : "sending mini diseqc A");
  if ((err = ioctl(fe_fd, FE_DISEQC_SEND_BURST, (i/4) % 2 ? SEC_MINI_B : SEC_MINI_A))) {
	tvhlog(LOG_ERR, "diseqc", "error sending mini diseqc command");
    return err;
  }
  msleep(15);

  /* set continuous tone */
  tvhtrace("diseqc", i % 2 ? "enabling continous tone" : "disabling continuous tone");
  if ((err = ioctl(fe_fd, FE_SET_TONE, i % 2 ? SEC_TONE_ON : SEC_TONE_OFF))) {
	tvhlog(LOG_ERR, "diseqc", "error setting continuous tone");
    return err;
  }
  return 0;
}

int
diseqc_voltage_off(int fe_fd)
{
  int err;
  
  tvhtrace("diseqc", "sending diseqc voltage off command");
  if ((err = ioctl(fe_fd, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF))) {
    tvhlog(LOG_ERR, "diseqc", "error sending diseqc voltage off command");
    return err;
  }
  return 0;
}

/*
 * input: same as diseqc_setup, plus EN50494 SCR#, frequency, PIN.
 * the SCR# is from 0 to 7, lnb_num is the "switch bank" for multi-input
 * SCR switches (satellite 0 or 1). If lnb_num is 2, this will send the
 * shutdown command to the SCR switch.
 * returns the real frequency the frontend needs to be tuned to
 * (usually uni_qrg +- a few khz)
 */
int
en50494_setup(int fe_fd, int lnb_num, int voltage, int band, int freq,
              int uni_scr, int uni_qrg, int uni_pin)
{
  __u8 frame, addr, cmd, d1, d2, d3, m_len;

  if (uni_scr < 0 || uni_scr > 7) {
    tvhlog(LOG_ERR, "en50494", "uni_scr out of range");
    return -1;
  }
  frame = 0xe0;		/* this is the "power down" command */
  addr  = 0x10;		/* only d1 (address << 5) needs to  */
  cmd   = 0x5a;		/* be filled in */
  d1 = d2 = d3 = 0x00;
  m_len = 5;
  unsigned int t = (freq / 1000 + uni_qrg + 2) / 4 - 350;
  if (lnb_num < 2 && t >= 1024) {
    tvhlog(LOG_ERR, "en50494", "OOPS: t >= 1024?");
    return -1;
  }
  if (uni_pin >= 0 && uni_pin < 0x100) {
    cmd   = 0x5c;
    d3    = uni_pin;
    m_len = 6;
  }
  int ret = (t + 350) * 4000 - freq;
  d1 = (uni_scr << 5);      /* adress */
  if (lnb_num < 2 && lnb_num >= 0) { /* lnb_num = 0/1 => tune, lnb_num = 2 => standby */
    d1 |= (t >> 8)       |  /* highest 3 bits of t */
          (lnb_num << 4) |  /* input 0/1 */
          (voltage << 3) |  /* horizontal == 0x08 */
          (band) << 2;      /* high_band  == 0x04 */
    d2 = t & 0xFF;
  } else if (freq != 0) {   /* paranoia check / message */
    tvhlog(LOG_ERR, "en50494", "lnb_num out of range 0-1 (%d) and freq != 0", lnb_num);
    return -1;
  }

  /* for debugging, can be disabled later */
  tvhlog(LOG_NOTICE, "en50494",
           "fd=%i, in=%i, v/h=%i, l/h=%i, f=%i, pin=%i qrg=%i scr=%d ret=%i",
           fe_fd, lnb_num, voltage, band, freq, uni_pin, uni_qrg, uni_scr, ret);

  /* just make sure tone is off */
  ioctl(fe_fd, FE_SET_TONE, SEC_TONE_OFF);

  if (ioctl(fe_fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18)) {
    tvhlog(LOG_ERR, "en50494", "error setting lnb voltage to 18V");
    return -1;
  }
  msleep(15); /* en50494 says: >4ms and < 22 ms */

  if (diseqc_send_msg(fe_fd, frame, addr, cmd, d1, d2, d3, m_len))
    return -1;
  msleep(50); /* en50494 says: >2ms and < 60 ms */

  if (ioctl(fe_fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13)) {
    tvhlog(LOG_ERR, "en50494", "error setting lnb voltage back to 13V");
    return -1;
  }
  return ret;
}
