#pragma once

#include "gxfp/algo/image/fpn.h"
#include "gxfp/algo/image/decoder.h"
#include "gxfp/flow/fdt.h"
#include "gxfp/io/dev.h"
#include "gxfp/tls/tls_service.h"

enum gxfp_session_mode {
    GXFP_MODE_CLOSED = 0,
    GXFP_MODE_READY,
    GXFP_MODE_ACTIVATING_TLS,
    GXFP_MODE_ACTIVATING_WAIT_UP,
    GXFP_MODE_ACTIVATING_DARK_CAPTURE,
    GXFP_MODE_IDLE,
    GXFP_MODE_WAIT_FINGER_ON,
    GXFP_MODE_CAPTURING,
    GXFP_MODE_WAIT_FINGER_OFF,
    GXFP_MODE_FAILED,
};

struct gxfp_session_impl {
    struct gxfp_dev dev;
    struct gxfp_tls_service svc;
    struct gxfp_fdt_flow fdt;

    uint8_t *psk;
    size_t psk_len;

    uint8_t *tap_io_buf;
    size_t tap_io_cap;

    enum gxfp_session_mode mode;

    struct gxfp_fpn_dark dark;
    int dark_loaded;

    struct gxfp_decoded_image pending_img;
    int pending_img_ready;

    struct {
        uint16_t tcode;
        uint16_t fdt_delta;
        uint8_t has_tcode_delta;
    } otp_ctx;
};

#define SESSION(s) ((struct gxfp_session_impl *) ((s)->impl))
