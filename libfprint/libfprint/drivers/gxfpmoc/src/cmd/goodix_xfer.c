#include "gxfp/cmd/goodix_xfer.h"

#include "gxfp/proto/goodix_proto.h"
#include "gxfp/proto/goodix_constants.h"
#include "gxfp/algo/common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int select_any_frame(const struct gxfp_frame_parsed *frame, void *ctx)
{
	(void)frame;
	(void)ctx;
	return 1;
}

int gxfp_goodix_send_async(struct gxfp_dev *dev, uint8_t cmd, const void *payload, uint16_t payload_len)
{
	uint8_t buf[1u + 2u + GOODIX_PAYLOAD_MAX + 1u];
	int frame_len;

	frame_len = gxfp_goodix_build_frame(cmd, (const uint8_t *)payload,
					    (size_t)payload_len, buf, sizeof(buf));
	if (frame_len < 0)
		return frame_len;
	return gxfp_dev_send_packet(dev, GOODIX_MP_TYPE_CMD, buf, (uint16_t)frame_len);
}

int gxfp_goodix_wait_selected(struct gxfp_dev *dev,
				 uint8_t expect_cmd,
				 void *rx,
				 uint32_t rx_cap,
				 uint32_t *out_rx_len,
				 int timeout_ms,
				 gxfp_goodix_frame_selector_fn selector,
				 void *selector_ctx)
{
	uint8_t tap_buf[sizeof(struct gxfp_tap_hdr) + (size_t)GXFP_IOCTL_TAP_PAYLOAD_MAX];
	int64_t deadline;
	int r;
	gxfp_goodix_frame_selector_fn fn = selector ? selector : select_any_frame;

	if (!dev || !rx || rx_cap == 0)
		return -EINVAL;
	if (out_rx_len)
		*out_rx_len = 0;

	deadline = gxfp_monotonic_ms() + (int64_t)(timeout_ms > 0 ? timeout_ms : 1000);
	for (;;) {
		struct gxfp_tap_hdr hdr;
		const uint8_t *rec_payload;
		size_t rec_payload_len;
		struct gxfp_frame_parsed frame;
		int sel;
		int64_t remain = deadline - gxfp_monotonic_ms();

		if (remain <= 0)
			return -ETIMEDOUT;

		r = gxfp_dev_poll_readable(dev, (int)remain);
		if (r == -EAGAIN)
			continue;
		if (r != 0)
			return r;

		r = (int)gxfp_dev_read_record(dev, tap_buf, sizeof(tap_buf), &hdr,
					     &rec_payload, &rec_payload_len);
		if (r < 0) {
			if (r == -EAGAIN)
				continue;
			return r;
		}

		if (hdr.type != GOODIX_MP_TYPE_CMD)
			continue;

		if (!gxfp_parse_goodix_body(rec_payload, rec_payload_len, &frame) || !frame.valid)
			continue;
		if (frame.cmd != expect_cmd) {
			continue;
		}

		sel = fn(&frame, selector_ctx);
		if (sel < 0)
			return sel;
		if (sel == 0) {
			continue;
		}

		if (rec_payload_len > rx_cap)
			return -EMSGSIZE;
		memcpy(rx, rec_payload, rec_payload_len);
		if (out_rx_len)
			*out_rx_len = (uint32_t)rec_payload_len;
		(void)hdr;
		return 0;
	}
}

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
				    void *selector_ctx)
{
	int attempt_max;
	int i;
	int r;

	if (!dev || !rx || rx_cap == 0)
		return -EINVAL;
	if (out_rx_len)
		*out_rx_len = 0;

	attempt_max = tries > 0 ? tries : 1;
	for (i = 0; i < attempt_max; i++) {
		r = gxfp_goodix_send_async(dev, req_cmd, payload, payload_len);
		if (r != 0)
			return r;

		r = gxfp_goodix_wait_selected(dev,
						 expect_cmd,
						 rx,
						 rx_cap,
						 out_rx_len,
						 timeout_ms,
						 selector,
						 selector_ctx);
		if (r == -ETIMEDOUT)
			continue;
		return r;
	}

	return -ETIMEDOUT;
}