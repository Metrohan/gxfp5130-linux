#include "gxfp/cmd/tls_cmd.h"

#include "gxfp/cmd/goodix_xfer.h"
#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/proto/goodix_proto.h"

#include <errno.h>

static int
gxfp_tls_cmd_send_expect_ack(struct gxfp_dev *dev,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len,
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
                                      1,
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

int gxfp_cmd_tls_server_init(struct gxfp_dev *dev)
{
    uint8_t payload[2] = { 0x00, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_goodix_send_async(dev,
                                      GXFP_CMD_TLS_SERVER_INIT,
                                      payload,
                                      (uint16_t)sizeof(payload));
}

int gxfp_cmd_get_image(struct gxfp_dev *dev)
{
    uint8_t payload[2] = { 0x01, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_goodix_send_async(dev,
                                      GXFP_CMD_GET_IMAGE,
                                      payload,
                                      (uint16_t)sizeof(payload));
}

int gxfp_cmd_tls_unlock(struct gxfp_dev *dev)
{
    uint8_t payload[2] = { 0x00, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_tls_cmd_send_expect_ack(dev,
                                        GXFP_CMD_TLS_UNLOCK,
                                        payload,
                                        (uint16_t)sizeof(payload),
                                        500);
}

int gxfp_cmd_tls_unlock_force(struct gxfp_dev *dev)
{
    uint8_t payload[2] = { 0x00, 0x00 };

    if (!dev)
        return -EINVAL;

    return gxfp_goodix_send_async(dev,
                                  GXFP_CMD_TLS_UNLOCK_FORCE,
                                  payload,
                                  (uint16_t)sizeof(payload));
}
