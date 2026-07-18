#include "gxfp/cmd/production_cmd.h"

#include "gxfp/cmd/goodix_xfer.h"
#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/algo/common.h"

#include <errno.h>
#include <string.h>

static int production_write_selector(const struct gxfp_frame_parsed *frame, void *ctx)
{
	(void)ctx;

	if (!frame || !frame->payload)
		return -EBADMSG;
	if (frame->payload_len < 1)
		return 0;
	return 1;
}

struct psk_read_ctx {
	uint32_t want_data_type;
};

static int preset_psk_selector(const struct gxfp_frame_parsed *frame, void *ctx)
{
	const struct psk_read_ctx *c = (const struct psk_read_ctx *)ctx;
	uint32_t data_type;
	uint32_t data_len;

	if (!frame || !frame->payload)
		return -EBADMSG;
	if (frame->payload_len < 9)
		return 0;
	if (frame->payload[0] != 0)
		return -EIO;

	data_type = gxfp_le32(frame->payload + 1);
	data_len = gxfp_le32(frame->payload + 5);
	if (data_len > (uint32_t)(frame->payload_len - 9))
		return -EBADMSG;
	if (c && c->want_data_type != 0 && data_type != c->want_data_type)
		return 0;

	return 1;
}

int gxfp_cmd_production_write_mcu(struct gxfp_dev *dev,
				 const uint8_t *blob,
				 uint16_t blob_len,
				 uint32_t *out_mcu_ret)
{
	uint8_t rx[GXFP_IOCTL_TAP_PAYLOAD_MAX];
	uint32_t rx_len = 0;
	struct gxfp_frame_parsed frame;
	int rc;

	if (!dev || !blob)
		return -EINVAL;
	if (out_mcu_ret)
		*out_mcu_ret = 0xffffffffu;

	rc = gxfp_goodix_request_selected(dev,
					     GXFP_CMD_PRODUCTION_WRITE_MCU,
					     GXFP_CMD_PRODUCTION_WRITE_MCU,
					     blob,
					     blob_len,
					     rx,
					     (uint32_t)sizeof(rx),
					     &rx_len,
					     4,
					     2000,
					     production_write_selector,
					     NULL);
	if (rc != 0)
		return rc;

	if (!gxfp_parse_goodix_body(rx, rx_len, &frame) || !frame.valid)
		return -EBADMSG;
	if (frame.cmd != GXFP_CMD_PRODUCTION_WRITE_MCU)
		return -EBADMSG;
	if (!frame.payload || frame.payload_len < 1)
		return -EBADMSG;

	if (out_mcu_ret)
		*out_mcu_ret = (uint32_t)frame.payload[0];
	return 0;
}

int gxfp_cmd_preset_psk_read(struct gxfp_dev *dev,
			    uint32_t data_type,
			    uint8_t *data,
			    uint32_t data_cap,
			    uint32_t *out_data_len)
{
	uint8_t req_payload[8];
	uint8_t rx[GXFP_IOCTL_TAP_PAYLOAD_MAX];
	uint32_t rx_len = 0;
	struct psk_read_ctx ctx;
	struct gxfp_frame_parsed frame;
	uint32_t payload_len;
	int rc;

	if (!dev || !data || data_cap == 0 || !out_data_len)
		return -EINVAL;
	*out_data_len = 0;

	memset(req_payload, 0, sizeof(req_payload));
	gxfp_le32enc(req_payload, data_type);
	ctx.want_data_type = data_type;

	rc = gxfp_goodix_request_selected(dev,
					     GXFP_CMD_PRESET_PSK_READ,
					     GXFP_CMD_PRESET_PSK_READ,
					     req_payload,
					     (uint16_t)sizeof(req_payload),
					     rx,
					     (uint32_t)sizeof(rx),
					     &rx_len,
					     4,
					     2000,
					     preset_psk_selector,
					     &ctx);
	if (rc != 0)
		return rc;

	if (!gxfp_parse_goodix_body(rx, rx_len, &frame) || !frame.valid)
		return -EBADMSG;
	if (frame.cmd != GXFP_CMD_PRESET_PSK_READ)
		return -EBADMSG;
	if (!frame.payload || frame.payload_len < 9)
		return -EBADMSG;
	if (frame.payload[0] != 0)
		return -EIO;
	if (gxfp_le32(frame.payload + 1) != data_type)
		return -ENODATA;

	payload_len = gxfp_le32(frame.payload + 5);
	if (payload_len > (uint32_t)(frame.payload_len - 9))
		return -EBADMSG;
	if (payload_len > data_cap)
		return -EMSGSIZE;

	memcpy(data, frame.payload + 9, payload_len);
	*out_data_len = payload_len;
	return 0;
}
