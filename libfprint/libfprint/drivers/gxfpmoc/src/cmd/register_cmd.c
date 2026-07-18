#include "gxfp/cmd/register_cmd.h"
#include "gxfp/cmd/goodix_xfer.h"
#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/proto/goodix_proto.h"

#include <errno.h>
#include <string.h>

int gxfp_cmd_read_reg(struct gxfp_dev *dev,
		      uint16_t reg,
		      uint8_t *out,
		      uint16_t out_len)
{
	uint8_t payload[5];
	uint8_t rx[GXFP_IOCTL_TAP_PAYLOAD_MAX];
	uint32_t rx_len = 0;
	struct gxfp_frame_parsed frame;
	int rc;

	if (!dev || !out || out_len == 0)
		return -EINVAL;

	payload[0] = 0x00;
	payload[1] = (uint8_t)(reg & 0xff);
	payload[2] = (uint8_t)((reg >> 8) & 0xff);
	payload[3] = (uint8_t)(out_len & 0xff);
	payload[4] = (uint8_t)((out_len >> 8) & 0xff);

	rc = gxfp_goodix_request_selected(dev,
					  GXFP_CMD_REG_READ,
					  GXFP_CMD_REG_READ,
					  payload,
					  (uint16_t)sizeof(payload),
					  rx,
					  (uint32_t)sizeof(rx),
					  &rx_len,
					  3,
					  500,
					  NULL,
					  NULL);
	if (rc != 0)
		return rc;

	if (!gxfp_parse_goodix_body(rx, rx_len, &frame))
		return -EBADMSG;

	if (!frame.payload || frame.payload_len < out_len)
		return -EBADMSG;

	memcpy(out, frame.payload, out_len);
	return 0;
}

int gxfp_cmd_read_chip_id(struct gxfp_dev *dev,
			  uint16_t *out_chip_id)
{
	uint8_t buf[4];
	int rc;

	if (!dev || !out_chip_id)
		return -EINVAL;
	*out_chip_id = 0;

	rc = gxfp_cmd_read_reg(dev, 0x0000, buf, 4);
	if (rc != 0)
		return rc;

	*out_chip_id = (uint16_t)((uint16_t)buf[1] |
				  ((uint16_t)buf[2] << 8));
	return 0;
}
