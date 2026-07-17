#include "gxfp/flow/device_recovery.h"

#include "gxfp/cmd/device_recovery_cmd.h"
#include "gxfp/cmd/tls_cmd.h"

#include <errno.h>

int
gxfp_device_recovery(struct gxfp_dev *dev,
			 int unstick_tls,
			 int reset_mcu,
			 int reset_sensor)
{
	int rr;
	int rc = 0;
	uint8_t reset_flag = 0;

	if (!dev)
		return -EINVAL;

	if (reset_mcu)
		reset_flag |= 0x02;
	if (reset_sensor)
		reset_flag |= 0x01;

	(void)gxfp_dev_flush_rxq(dev);

	rr = gxfp_cmd_notify_power_state(dev);
	(void)rr;

	if (unstick_tls) {
		rr = gxfp_cmd_d01(dev);
		(void)rr;

		rr = gxfp_cmd_send_nop(dev);
		(void)rr;

		rr = gxfp_cmd_tls_unlock(dev);
		(void)rr;
	}

	rr = gxfp_cmd_set_sleep_mode(dev);
	(void)rr;

	if (reset_flag != 0) {
		rr = gxfp_cmd_reset_device(dev, reset_flag);
		if (rr != 0 && rc == 0)
			rc = rr;
	}

	(void)gxfp_dev_flush_rxq(dev);
	return rc;
}