#ifndef __DISEQC_H__
#define __DISEQC_H__

#include <stdint.h>
#include <linux/dvb/frontend.h>

#if defined(PLATFORM_FREEBSD)
#define 	__u8   u_int8_t
#endif

/**
 *   set up the switch to position/voltage/tone
 */
int diseqc_send_msg(int fe_fd, __u8 framing_byte, __u8 address, __u8 cmd,
                    __u8 data_1, __u8 data_2, __u8 data_3, __u8 msg_len);
int diseqc_setup(int fe_fd, int lnb_num, int voltage, int band,
                  uint32_t version, uint32_t repeats);
int diseqc_voltage_off(int fe_fd);

#endif
