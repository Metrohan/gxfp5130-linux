#pragma once

#include "gxfp/io/dev.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gxfp_cmd_tls_server_init(struct gxfp_dev *dev);
int gxfp_cmd_get_image(struct gxfp_dev *dev);
int gxfp_cmd_tls_unlock(struct gxfp_dev *dev);
int gxfp_cmd_tls_unlock_force(struct gxfp_dev *dev);

#ifdef __cplusplus
}
#endif
