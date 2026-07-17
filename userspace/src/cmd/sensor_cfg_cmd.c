#include "gxfp/cmd/sensor_cfg_cmd.h"

#include "gxfp/cmd/goodix_xfer.h"
#include "gxfp/proto/goodix_cmd.h"
#include "gxfp/proto/goodix_proto.h"

#include <errno.h>

int gxfp_cmd_read_otp(struct gxfp_dev *dev,
                      uint8_t *otp,
                      uint16_t otp_cap,
                      uint16_t *out_otp_len)
{
    uint8_t req_payload[2] = { 0x00, 0x00 };
    uint8_t rx[GXFP_IOCTL_TAP_PAYLOAD_MAX];
    uint32_t rx_len = 0;
    struct gxfp_frame_parsed frame;
    int rc;

    if (!dev || !otp || otp_cap == 0)
        return -EINVAL;
    if (out_otp_len)
        *out_otp_len = 0;

    rc = gxfp_goodix_request_selected(dev,
                                      GXFP_CMD_READ_OTP,
                                      GXFP_CMD_READ_OTP,
                                      req_payload,
                                      (uint16_t)sizeof(req_payload),
                                      rx,
                                      (uint32_t)sizeof(rx),
                                      &rx_len,
                                      8,
                                      500,
                                      NULL,
                                      NULL);
    if (rc != 0)
        return rc;

    if (!gxfp_parse_goodix_body(rx, rx_len, &frame))
        return -EBADMSG;
    if (!frame.payload)
        return -EBADMSG;
    if (frame.payload_len > otp_cap)
        return -EMSGSIZE;

    for (uint16_t i = 0; i < frame.payload_len; i++)
        otp[i] = frame.payload[i];

    if (out_otp_len)
        *out_otp_len = frame.payload_len;

    return 0;
}

int gxfp_cmd_upload_config_mcu(struct gxfp_dev *dev,
                               const uint8_t *cfg,
                               uint16_t cfg_len,
                               uint8_t *out_ack_status)
{
    uint8_t rx[GXFP_IOCTL_TAP_PAYLOAD_MAX];
    uint32_t rx_len = 0;
    struct gxfp_frame_parsed frame;
    int rc;

    if (!dev || !cfg || cfg_len == 0)
        return -EINVAL;
    if (out_ack_status)
        *out_ack_status = 0xff;

    rc = gxfp_goodix_request_selected(dev,
                                      GXFP_CMD_UPLOAD_CONFIG_MCU,
                                      GXFP_CMD_ACK,
                                      cfg,
                                      cfg_len,
                                      rx,
                                      (uint32_t)sizeof(rx),
                                      &rx_len,
                                      2,
                                      2000,
                                      NULL,
                                      NULL);
    if (rc != 0)
        return rc;

    if (!gxfp_parse_goodix_body(rx, rx_len, &frame))
        return -EBADMSG;

    if (out_ack_status)
        *out_ack_status = (frame.payload && frame.payload_len > 0) ? frame.payload[0] : 0x00;

    return 0;
}
