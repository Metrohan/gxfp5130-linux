#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gxfp/io/dev.h"
#include "gxfp/io/uapi.h"
#include "gxfp/cmd/fdt_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

enum gxfp_fdt_mode {
	GXFP_FDT_MODE_IDLE = 0,
	GXFP_FDT_MODE_WAIT_DOWN,
	GXFP_FDT_MODE_WAIT_UP,
};

enum gxfp_fdt_state {
	GXFP_FDT_STATE_UNKNOWN = 0,
	GXFP_FDT_STATE_UP,
	GXFP_FDT_STATE_DOWN,
};

enum gxfp_fdt_event {
	GXFP_FDT_EVENT_NONE = 0,
	GXFP_FDT_EVENT_DOWN = 1u << 0,
	GXFP_FDT_EVENT_UP = 1u << 1,
	GXFP_FDT_EVENT_REVERSE = 1u << 2,
};

struct gxfp_fdt_flow {
	struct gxfp_cmd_fdt_state cmd;
	enum gxfp_fdt_mode mode;
	enum gxfp_fdt_state state;
	uint16_t last_status;
	uint8_t armed_up_table[24];
	uint8_t last_good_up_table[24];
	int last_good_up_valid;
	int wait_up_retries;
	int64_t wait_up_armed_ms;
};

void gxfp_fdt_flow_init(struct gxfp_fdt_flow *flow);

int gxfp_fdt_flow_set_mode(struct gxfp_fdt_flow *flow,
			   struct gxfp_dev *dev,
			   enum gxfp_fdt_mode mode);

int gxfp_fdt_flow_feed_record(struct gxfp_fdt_flow *flow,
			      struct gxfp_dev *dev,
			      const struct gxfp_tap_hdr *tap_hdr,
			      const uint8_t *payload,
			      size_t payload_len,
			      uint32_t *out_events);

int gxfp_fdt_flow_poll(struct gxfp_fdt_flow *flow,
		       struct gxfp_dev *dev,
		       int64_t now_ms);

int gxfp_fdt_flow_wait_up_retry_due(const struct gxfp_fdt_flow *flow,
				    int64_t now_ms);

enum gxfp_fdt_state gxfp_fdt_flow_get_state(const struct gxfp_fdt_flow *flow);

#ifdef __cplusplus
}
#endif
