// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>

#include "gxfp_delay.h"

void gxfp_busy_wait_us(unsigned int us)
{
	if (!us)
		return;
	if (us <= 2000) {
		udelay(us);
		return;
	}
	usleep_range(us, us + 200);
}
