#pragma once

#include "gxfp/io/dev.h"

#include <stdint.h>

#define GXFP_DT_BB010002 0xBB010002u
#define GXFP_DT_BB010003 0xBB010003u

#ifdef __cplusplus
extern "C" {
#endif

int gxfp_cmd_production_write_mcu(struct gxfp_dev *dev,
				 const uint8_t *blob,
				 uint16_t blob_len,
				 uint32_t *out_mcu_ret);

int gxfp_cmd_preset_psk_read(struct gxfp_dev *dev,
			    uint32_t data_type,
			    uint8_t *data,
			    uint32_t data_cap,
			    uint32_t *out_data_len);

#ifdef __cplusplus
}
#endif
