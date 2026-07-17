#include "gxfp/flow/fdt.h"

#include "gxfp/cmd/fdt_cmd.h"
#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/proto/goodix_constants.h"
#include "gxfp/proto/goodix_proto.h"
#include "gxfp/algo/common.h"

#include <errno.h>
#include <string.h>

#define GXFP_FDT_BASE_TABLE_LEN 24
#define GXFP_FDT_BASE_UPDATE_ENTRIES 10

#define FDT_STATUS_DOWN_GET_UP_BASE 0x0002
#define FDT_STATUS_UP 0x0200
#define FDT_STATUS_IRQ_DOWN_GET_UP_BASE 0x0002
#define FDT_STATUS_IRQ_UP_GET_DOWN_BASE 0x0200
#define FDT_STATUS_IRQ_REVERSE_0 0x0080
#define FDT_STATUS_IRQ_REVERSE_2 0x0082

#define FDT_UP_DEFAULT_OFFSET 0x15u
#define FDT_UP_NOTOUCH_FALLBACK 0x1300u
#define FDT_WAIT_UP_RETRY_MS 750
#define FDT_WAIT_UP_MAX_RETRIES 3

static void fdt_downbase_from_raw(const uint8_t *raw_base,
				  size_t raw_len,
				  uint8_t out_table[GXFP_FDT_BASE_TABLE_LEN])
{
	size_t i;
	size_t entries;

	if (!raw_base || !out_table || raw_len < 2)
		return;

	entries = raw_len / 2;
	if (entries > GXFP_FDT_BASE_UPDATE_ENTRIES)
		entries = GXFP_FDT_BASE_UPDATE_ENTRIES;

	for (i = 0; i < entries; i++) {
		uint16_t raw = gxfp_le16(raw_base + (i * 2));
		uint16_t half = (uint16_t)((raw >> 1) & 0xffffu);
		uint16_t encoded = (uint16_t)((half << 8) | 0x80u);

		out_table[(i * 2) + 0] = (uint8_t)(encoded & 0xffu);
		out_table[(i * 2) + 1] = (uint8_t)((encoded >> 8) & 0xffu);
	}
}

static void fdt_upbase_from_raw(uint16_t touchflag,
				int second_flag,
				int diff_use,
				int16_t dac_offset,
				const uint8_t *raw_base,
				size_t raw_len,
				uint8_t out_table[GXFP_FDT_BASE_TABLE_LEN])
{
	size_t i;
	size_t entries;
	uint8_t merged[GXFP_FDT_BASE_TABLE_LEN];

	if (!raw_base || !out_table || raw_len < 2)
		return;

	entries = raw_len / 2;
	if (entries > GXFP_FDT_BASE_UPDATE_ENTRIES)
		entries = GXFP_FDT_BASE_UPDATE_ENTRIES;

	memcpy(merged, out_table, sizeof(merged));

	for (i = 0; i < entries; i++) {
		uint16_t raw = gxfp_le16(raw_base + (i * 2));
		uint16_t half = (uint16_t)((raw >> 1) & 0xffffu);
		uint16_t offset = (dac_offset == 0) ? FDT_UP_DEFAULT_OFFSET : (uint16_t)dac_offset;
		uint16_t encoded = (uint16_t)(((half + offset) << 8) | (half + offset));
		uint16_t prev;

		if (second_flag == 0 && ((touchflag >> i) & 0x1u) == 0) {
			if (dac_offset == 0)
				encoded = (uint16_t)((encoded >> 8) | FDT_UP_NOTOUCH_FALLBACK);
			else
				encoded = (uint16_t)((((uint16_t)(dac_offset - 2)) << 8) | (encoded >> 8));
		}

		prev = gxfp_le16(merged + (i * 2));
		if (diff_use == 1 && encoded > prev)
			encoded = prev;

		merged[(i * 2) + 0] = (uint8_t)(encoded & 0xffu);
		merged[(i * 2) + 1] = (uint8_t)((encoded >> 8) & 0xffu);
	}

	memcpy(out_table, merged, sizeof(merged));
}

static void fdt_base_table_update_from_frame(struct gxfp_cmd_fdt_state *state,
					     const struct gxfp_frame_parsed *frame)
{
	uint16_t irq_status;

	if (!state || !frame)
		return;

	irq_status = gxfp_le16(frame->payload);

	if (irq_status == FDT_STATUS_IRQ_DOWN_GET_UP_BASE) {
		const uint8_t *raw_base;
		size_t raw_len;
		uint16_t touchflag;
		int second_flag = state->up_second_flag;

		if (frame->payload_len < 4)
			return;

		touchflag = gxfp_le16(frame->payload + 2);
		raw_base = frame->payload + 4;
		raw_len = frame->payload_len - 4;

		if (state->up_need_twice) {
			second_flag = (state->up_round > 0) ? 1 : 0;
			state->up_round ^= 1;
		}

		fdt_upbase_from_raw(touchflag,
				   second_flag,
				   state->up_diff_use,
				   state->up_dac_offset,
				   raw_base,
				   raw_len,
				   state->up_table_5130);
		return;
	}

	if (irq_status == FDT_STATUS_IRQ_UP_GET_DOWN_BASE ||
	    irq_status == FDT_STATUS_IRQ_REVERSE_0 ||
	    irq_status == FDT_STATUS_IRQ_REVERSE_2) {
		const uint8_t *raw_base;
		size_t raw_len;

		if (frame->payload_len < 4)
			return;

		raw_base = frame->payload + 4;
		raw_len = frame->payload_len - 4;
		if (raw_len > (GXFP_FDT_BASE_UPDATE_ENTRIES * 2))
			raw_len = GXFP_FDT_BASE_UPDATE_ENTRIES * 2;

		fdt_downbase_from_raw(raw_base, raw_len, state->down_table_5130);
	}
}

static int fdt_update_state_from_record(struct gxfp_cmd_fdt_state *state,
					const struct gxfp_tap_hdr *tap_hdr,
					const uint8_t *payload,
					size_t payload_len,
					uint16_t *out_status)
{
	struct gxfp_frame_parsed frame;

	if (!state)
		return -EINVAL;

	if (!tap_hdr || !payload)
		return -EINVAL;

	if (tap_hdr->type != GOODIX_MP_TYPE_CMD &&
	    tap_hdr->type != GOODIX_MP_TYPE_NOTICE)
		return -ENOENT;

	if (!gxfp_parse_goodix_body(payload, payload_len, &frame) || !frame.valid)
		return -ENOENT;

	if (frame.payload_len < 2)
		return -ENOENT;

	if (frame.cmd != GXFP_CMD_FDT_STATUS &&
	    frame.cmd != GXFP_CMD_FDT_DOWN &&
	    frame.cmd != GXFP_CMD_FDT_MODE &&
	    frame.cmd != GXFP_CMD_FDT_UP)
		return -ENOENT;

	if (!state->base_table_inited)
		gxfp_cmd_fdt_state_init(state);

	fdt_base_table_update_from_frame(state, &frame);

	if (out_status)
		*out_status = gxfp_le16(frame.payload);

	return 0;
}

void gxfp_fdt_flow_init(struct gxfp_fdt_flow *flow)
{
	if (!flow)
		return;

	memset(flow, 0, sizeof(*flow));
	gxfp_cmd_fdt_state_init(&flow->cmd);
	flow->mode = GXFP_FDT_MODE_IDLE;
	flow->state = GXFP_FDT_STATE_UNKNOWN;
}

int gxfp_fdt_flow_set_mode(struct gxfp_fdt_flow *flow,
			   struct gxfp_dev *dev,
			   enum gxfp_fdt_mode mode)
{
	int r;

	if (!flow || !dev)
		return -EINVAL;
	if (mode == flow->mode)
		return 0;

	if (mode == GXFP_FDT_MODE_IDLE) {
		flow->mode = GXFP_FDT_MODE_IDLE;
		return 0;
	}

	if (mode == GXFP_FDT_MODE_WAIT_DOWN) {
		r = gxfp_cmd_fdt_set_mode(dev, &flow->cmd);
		if (r < 0)
			return r;
		r = gxfp_cmd_fdt_send_down(dev, &flow->cmd);
		if (r < 0)
			return r;
		flow->mode = GXFP_FDT_MODE_WAIT_DOWN;
		return 0;
	}

	if (mode == GXFP_FDT_MODE_WAIT_UP) {
		r = gxfp_cmd_fdt_set_mode(dev, &flow->cmd);
		if (r < 0)
			return r;
		r = gxfp_cmd_fdt_send_up(dev, &flow->cmd);
		if (r < 0)
			return r;
		memcpy(flow->armed_up_table, flow->cmd.up_table_5130,
		       sizeof(flow->armed_up_table));
		flow->wait_up_armed_ms = gxfp_monotonic_ms();
		flow->wait_up_retries = 0;
		flow->mode = GXFP_FDT_MODE_WAIT_UP;
		return 0;
	}

	return -EINVAL;
}

static uint32_t fdt_status_to_events(uint16_t status)
{
	if (status == FDT_STATUS_DOWN_GET_UP_BASE)
		return GXFP_FDT_EVENT_DOWN;
	if (status == FDT_STATUS_UP)
		return GXFP_FDT_EVENT_UP;
	if (status == FDT_STATUS_IRQ_REVERSE_0 || status == FDT_STATUS_IRQ_REVERSE_2)
		return GXFP_FDT_EVENT_REVERSE;

	return GXFP_FDT_EVENT_NONE;
}

int gxfp_fdt_flow_feed_record(struct gxfp_fdt_flow *flow,
			      struct gxfp_dev *dev,
			      const struct gxfp_tap_hdr *tap_hdr,
			      const uint8_t *payload,
			      size_t payload_len,
			      uint32_t *out_events)
{
	uint16_t status = 0;
	uint32_t events;
	int r;

	if (!flow || !dev || !tap_hdr || !payload || !out_events)
		return -EINVAL;

	*out_events = GXFP_FDT_EVENT_NONE;
	r = fdt_update_state_from_record(&flow->cmd, tap_hdr, payload, payload_len, &status);
	if (r < 0)
		return (r == -EAGAIN || r == -ENOENT) ? -EAGAIN : r;

	flow->last_status = status;
	events = fdt_status_to_events(status);

	if ((events & GXFP_FDT_EVENT_REVERSE) && flow->mode == GXFP_FDT_MODE_WAIT_DOWN) {
		r = gxfp_cmd_fdt_set_mode(dev, &flow->cmd);
		if (r < 0)
			return r;
		r = gxfp_cmd_fdt_send_down(dev, &flow->cmd);
		if (r < 0)
			return r;
	}

	if (events & GXFP_FDT_EVENT_DOWN)
		flow->state = GXFP_FDT_STATE_DOWN;
	if (events & GXFP_FDT_EVENT_UP) {
		flow->state = GXFP_FDT_STATE_UP;
		memcpy(flow->last_good_up_table, flow->armed_up_table,
		       sizeof(flow->last_good_up_table));
		flow->last_good_up_valid = 1;
	}

	*out_events = events;
	if (events == GXFP_FDT_EVENT_NONE)
		return -EAGAIN;

	return 0;
}

int gxfp_fdt_flow_poll(struct gxfp_fdt_flow *flow,
		       struct gxfp_dev *dev,
		       int64_t now_ms)
{
	int due;
	int r;

	if (!flow || !dev)
		return -EINVAL;

	due = gxfp_fdt_flow_wait_up_retry_due(flow, now_ms);
	if (due <= 0)
		return due;

	if (flow->last_good_up_valid)
		memcpy(flow->cmd.up_table_5130, flow->last_good_up_table,
		       sizeof(flow->cmd.up_table_5130));

	r = gxfp_cmd_fdt_set_mode(dev, &flow->cmd);
	if (r < 0)
		return r;
	r = gxfp_cmd_fdt_send_up(dev, &flow->cmd);
	if (r < 0)
		return r;

	memcpy(flow->armed_up_table, flow->cmd.up_table_5130,
	       sizeof(flow->armed_up_table));
	flow->wait_up_armed_ms = now_ms;
	flow->wait_up_retries++;
	return 0;
}

int gxfp_fdt_flow_wait_up_retry_due(const struct gxfp_fdt_flow *flow,
				    int64_t now_ms)
{
	if (!flow)
		return -EINVAL;
	if (flow->mode != GXFP_FDT_MODE_WAIT_UP || flow->state == GXFP_FDT_STATE_UP)
		return 0;
	if (flow->wait_up_armed_ms <= 0 || now_ms < flow->wait_up_armed_ms)
		return 0;
	if (now_ms - flow->wait_up_armed_ms < FDT_WAIT_UP_RETRY_MS)
		return 0;
	if (flow->wait_up_retries >= FDT_WAIT_UP_MAX_RETRIES)
		return -ETIMEDOUT;
	return 1;
}

enum gxfp_fdt_state gxfp_fdt_flow_get_state(const struct gxfp_fdt_flow *flow)
{
	if (!flow)
		return GXFP_FDT_STATE_UNKNOWN;

	return flow->state;
}
