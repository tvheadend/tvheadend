#ifndef __DISEQC_H__
#define __DISEQC_H__

#include <stdint.h>
#include <linux/dvb/frontend.h>


struct diseqc_cmd {
	struct dvb_diseqc_master_cmd cmd;
	uint32_t wait;
};


extern int diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd **cmd,
			   fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b);


/**
 *   set up the switch to position/voltage/tone
 */
int diseqc_setup(int frontend_fd, int switch_pos, int voltage_18, int hiband,
		 int diseqc_ver);

int diseqc_voltage_off(int frontend_fd);

#endif
