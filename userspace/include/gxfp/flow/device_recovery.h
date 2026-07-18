#pragma once

#include "gxfp/io/dev.h"

#ifdef __cplusplus
extern "C" {
#endif

int gxfp_device_recovery(struct gxfp_dev *dev,
			      int unstick_tls,
			      int reset_mcu,
			      int reset_sensor);

#ifdef __cplusplus
}
#endif
