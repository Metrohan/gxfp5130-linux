#pragma once

#include "gxfp/io/dev.h"
#include "gxfp/proto/goodix_proto.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gxfp_goodix_send_async(struct gxfp_dev *dev,
			      uint8_t cmd,
			      const void *payload,
			      uint16_t payload_len);

typedef int (*gxfp_goodix_frame_selector_fn)(const struct gxfp_frame_parsed *frame,
					      void *ctx);

int gxfp_goodix_wait_selected(struct gxfp_dev *dev,
				 uint8_t expect_cmd,
				 void *rx,
				 uint32_t rx_cap,
				 uint32_t *out_rx_len,
				 int timeout_ms,
				 gxfp_goodix_frame_selector_fn selector,
				 void *selector_ctx);

int gxfp_goodix_request_selected(struct gxfp_dev *dev,
				    uint8_t req_cmd,
				    uint8_t expect_cmd,
				    const void *payload,
				    uint16_t payload_len,
				    void *rx,
				    uint32_t rx_cap,
				    uint32_t *out_rx_len,
				    int tries,
				    int timeout_ms,
				    gxfp_goodix_frame_selector_fn selector,
				    void *selector_ctx);

#ifdef __cplusplus
}
#endif