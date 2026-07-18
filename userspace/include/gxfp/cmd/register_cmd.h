#pragma once

#include "gxfp/io/dev.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int gxfp_cmd_read_reg(struct gxfp_dev *dev,
		      uint16_t reg,
		      uint8_t *out,
		      uint16_t out_len);

int gxfp_cmd_read_chip_id(struct gxfp_dev *dev,
			  uint16_t *out_chip_id);

#ifdef __cplusplus
}
#endif
