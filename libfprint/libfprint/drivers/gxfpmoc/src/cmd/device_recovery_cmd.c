#include "gxfp/cmd/device_recovery_cmd.h"

#include "gxfp/cmd/goodix_xfer.h"
#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/proto/goodix_proto.h"

#include <errno.h>

#define GXFP_RESET_DELAY_MAGIC 0x14
#define GXFP_RECOVERY_CMD_TIMEOUT_MS 500

static int
gxfp_cmd_send_expect_ack(struct gxfp_dev *dev,
                         uint8_t cmd,
                         const uint8_t *payload,
                         uint16_t payload_len,
                         int tries,
                         int timeout_ms)
{
    uint8_t rx[GXFP_IOCTL_TAP_PAYLOAD_MAX];
    uint32_t rx_len = 0;
    struct gxfp_frame_parsed frame;
    int rc;

    rc = gxfp_goodix_request_selected(dev,
                                      cmd,
                                      GXFP_CMD_ACK,
                                      payload,
                                      payload_len,
                                      rx,
                                      (uint32_t)sizeof(rx),
                                      &rx_len,
                                      tries,
                                      timeout_ms,
                                      NULL,
                                      NULL);
    if (rc != 0)
        return rc;

    if (!gxfp_parse_goodix_body(rx, rx_len, &frame) || !frame.valid)
        return -EBADMSG;
    if (frame.cmd != GXFP_CMD_ACK)
        return -EBADMSG;

    return 0;
}

int gxfp_cmd_notify_power_state(struct gxfp_dev *dev)
{
    const uint8_t power_state = 0x01;
    uint8_t payload[1] = { power_state };

    if (!dev)
        return -EINVAL;

    return gxfp_cmd_send_expect_ack(dev,
                                    GXFP_CMD_NOTIFY_POWER_STATE,
                                    payload,
                                    (uint16_t)sizeof(payload),
                                    1,
                                    GXFP_RECOVERY_CMD_TIMEOUT_MS);
}

int gxfp_cmd_send_nop(struct gxfp_dev *dev)
{
    uint8_t payload[4] = { 0x00, 0x00, 0x00, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_cmd_send_expect_ack(dev,
                                    GXFP_CMD_SEND_NOP,
                                    payload,
                                    (uint16_t)sizeof(payload),
                                    1,
                                    GXFP_RECOVERY_CMD_TIMEOUT_MS);
}

int gxfp_cmd_set_sleep_mode(struct gxfp_dev *dev)
{
    uint8_t payload[2] = { 0x00, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_cmd_send_expect_ack(dev,
                                    GXFP_CMD_SET_SLEEP_MODE,
                                    payload,
                                    (uint16_t)sizeof(payload),
                                    1,
                                    GXFP_RECOVERY_CMD_TIMEOUT_MS);
}

int gxfp_cmd_d01(struct gxfp_dev *dev)
{
    uint8_t payload[2] = { 0x00, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_cmd_send_expect_ack(dev,
                                    GXFP_CMD_D01,
                                    payload,
                                    (uint16_t)sizeof(payload),
                                    1,
                                    GXFP_RECOVERY_CMD_TIMEOUT_MS);
}

int gxfp_cmd_reset_device(struct gxfp_dev *dev, uint8_t reset_flag)
{
    uint8_t payload[2] = { reset_flag, GXFP_RESET_DELAY_MAGIC };

    if (!dev)
        return -EINVAL;

    return gxfp_cmd_send_expect_ack(dev,
                                    GXFP_CMD_RESET_DEVICE,
                                    payload,
                                    (uint16_t)sizeof(payload),
                                    6,
                                    GXFP_RECOVERY_CMD_TIMEOUT_MS);
}
