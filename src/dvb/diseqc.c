#include <time.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>

//#include "scan.h"
#include "diseqc.h"


struct diseqc_cmd switch_cmds[] = {
	{ { { 0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf2, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf1, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf3, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf4, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf6, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf5, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf7, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf8, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfa, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf9, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfb, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfc, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfe, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfd, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xff, 0x00, 0x00 }, 4 }, 0 }
};


/*--------------------------------------------------------------------------*/

static inline
void msleep(uint32_t msec)
{
	struct timespec req = { msec / 1000, 1000000 * (msec % 1000) };

	while (nanosleep(&req, &req))
		;
}

#define printf(x...)


int diseqc_send_msg (int fd, fe_sec_voltage_t v, struct diseqc_cmd **cmd,
		     fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
	int err;

	if ((err = ioctl(fd, FE_SET_TONE, SEC_TONE_OFF)))
		return err;

	if ((err = ioctl(fd, FE_SET_VOLTAGE, v)))
		return err;

	msleep(15);
	while (*cmd) {
		if ((err = ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd)))
			return err;

		msleep((*cmd)->wait);
		cmd++;
	}

	msleep(15);

	if ((err = ioctl(fd, FE_DISEQC_SEND_BURST, b)))
		return err;

	msleep(15);

	return ioctl(fd, FE_SET_TONE, t);
}


int diseqc_setup(int frontend_fd, int switch_pos, int voltage_18, int hiband)
{
	struct diseqc_cmd *cmd[2] = { NULL, NULL };
	int i = 4 * switch_pos + 2 * hiband + (voltage_18 ? 1 : 0);


	if(i < 0 || i >= (int) (sizeof(switch_cmds)/sizeof(struct diseqc_cmd)))
		return -1;

	cmd[0] = &switch_cmds[i];

	return diseqc_send_msg (frontend_fd,
				i % 2 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
				cmd,
				(i/2) % 2 ? SEC_TONE_ON : SEC_TONE_OFF,
				(i/4) % 2 ? SEC_MINI_B : SEC_MINI_A);
}


