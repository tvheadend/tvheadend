#ifndef __DISEQC_H__
#define __DISEQC_H__

#include <stdint.h>
#include <linux/dvb/frontend.h>

/**
 *   set up the switch to position/voltage/tone
 */
int diseqc_setup(int fe_fd, int input, int voltage, int band, int diseqc_ver);
int diseqc_voltage_off(int fe_fd);

#endif
