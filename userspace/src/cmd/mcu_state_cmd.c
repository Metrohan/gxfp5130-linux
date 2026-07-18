#include "gxfp/cmd/mcu_state_cmd.h"

#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/proto/goodix_proto.h"
#include "gxfp/io/dev.h"
#include "gxfp/cmd/goodix_xfer.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int gxfp_mcu_state_parse(const uint8_t *buf, size_t len,
			 struct gxfp_mcu_state *out)
{
	if (!buf || !out)
		return -EINVAL;
	memset(out, 0, sizeof(*out));
	if (len < GXFP_MCU_STATE_LEN)
		return -EBADMSG;

	/* byte 0 */
	out->version = buf[0];
	out->b1_raw = buf[1];

	/* byte 1 – bit-flags */
	out->is_pov_image_valid = (buf[1] >> 0) & 1;
	out->is_tls_connected   = (buf[1] >> 1) & 1;
	out->is_locked          = (buf[1] >> 3) & 1;

	/* byte 2 – captured_num:%d(%d) */
	out->avail_img_cnt = buf[2] & 0x0F;
	out->pov_img_cnt   = (buf[2] >> 4) & 0x0F;

	/* bytes 3-8 */
	memcpy(out->raw_3_8, &buf[3], sizeof(out->raw_3_8));

	/* byte 9 – ec_falling_count & per-bit flags */
	out->ec_falling_count  = buf[9];
	out->flags9_pba         = (buf[9] >> 4) & 1;
	out->flags9_tls_session = (buf[9] >> 5) & 1;
	out->flags9_bit6        = (buf[9] >> 6) & 1;

	/* bytes 10-14 */
	memcpy(out->raw_10_14, &buf[10], sizeof(out->raw_10_14));

	/* byte 15 */
	out->config_down_flag = buf[15];

	/* bytes 16-19 */
	memcpy(out->raw_16_19, &buf[16], sizeof(out->raw_16_19));

	return 0;
}

static void build_trigger_payload(uint8_t out[5])
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	uint16_t ts_u16 = (uint16_t)(((uint64_t)(ts.tv_sec % 60) * 1000ULL)
				     + ((uint64_t)ts.tv_nsec / 1000000ULL));
	out[0] = 0x55;
	out[1] = (uint8_t)(ts_u16 & 0xff);
	out[2] = (uint8_t)((ts_u16 >> 8) & 0xff);
	out[3] = 0x00;
	out[4] = 0x00;
}

int gxfp_mcu_state_query(struct gxfp_dev *dev,
			      struct gxfp_mcu_state *out_state)
{
	uint8_t trigger[5];
	uint8_t *rx = NULL;
	uint32_t rx_len = 0;
	struct gxfp_frame_parsed frame;
	int r;

	if (!dev || !out_state)
		return -EINVAL;

	rx = (uint8_t *)malloc(4096);
	if (!rx)
		return -ENOMEM;

	(void)gxfp_dev_flush_rxq(dev);
	build_trigger_payload(trigger);
	r = gxfp_goodix_request_selected(dev,
				       GXFP_CMD_TRIGGER_MCU_STATE,
				       GXFP_CMD_QUERY_MCU_STATE,
				       trigger,
				       (uint16_t)sizeof(trigger),
				       rx,
				       4096,
				       &rx_len,
					   1,
				       500,
					   NULL,
					   NULL);
	if (r != 0) {
		free(rx);
		return r;
	}

	if (!gxfp_parse_goodix_body(rx, (size_t)rx_len, &frame) ||
	    !frame.valid || frame.cmd != GXFP_CMD_QUERY_MCU_STATE ||
	    !frame.payload || frame.payload_len < GXFP_MCU_STATE_LEN) {
		free(rx);
		return -EBADMSG;
	}

	r = gxfp_mcu_state_parse(frame.payload, frame.payload_len, out_state);
	free(rx);
	return r;
}
