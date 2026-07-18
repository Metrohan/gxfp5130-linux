#include "gxfp/flow/session.h"

#include "gxfp/algo/image/fpn.h"
#include "gxfp/algo/common.h"
#include "gxfp/algo/sensor_cfg.h"
#include "gxfp/cmd/device_recovery_cmd.h"
#include "gxfp/cmd/register_cmd.h"
#include "gxfp/cmd/sensor_cfg_cmd.h"
#include "gxfp/flow/fdt.h"
#include "gxfp/flow/device_recovery.h"
#include "gxfp/io/dev.h"
#include "gxfp/tls/tls_service.h"

#include "session_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum gxfp_cleanup_goal {
    GXFP_CLEANUP_DEACTIVATE = 0,
    GXFP_CLEANUP_CLOSE,
};

static int
session_ensure_impl(struct gxfp_session *s)
{
    if (!s)
        return -EINVAL;
    if (s->impl)
        return 0;

    s->impl = calloc(1, sizeof(struct gxfp_session_impl));
    if (!s->impl)
        return -ENOMEM;

    gxfp_fdt_flow_init(&SESSION(s)->fdt);
    SESSION(s)->mode = GXFP_MODE_CLOSED;
    return 0;
}

static void session_set_error(struct gxfp_session *s,
                              struct gxfp_session_events *ev,
                              int err,
                              enum gxfp_session_error_target target,
                              const char *fmt,
                              ...) __attribute__((format(printf, 5, 6)));
static int gxfp_session_mode_has_io(const struct gxfp_session *s);
static void gxfp_session_stop_run(struct gxfp_session *s);
static void gxfp_session_reset_runtime(struct gxfp_session *s);
static void session_apply_fdt_runtime_from_otp(struct gxfp_session *s);
static int session_apply_milanl_cfg(struct gxfp_session *s);

static int
session_apply_milanl_cfg(struct gxfp_session *s)
{
    uint8_t otp[256];
    uint16_t otp_len = 0;
    struct gxfp_milanl_otp_cfg otp_cfg;
    const uint8_t *template_data = NULL;
    size_t template_len = 0;
    uint8_t cfg_blob[GXFP_MILANL_CFG_LEN];
    int cfg_blob_len;
    uint8_t ack_status = 0xff;
    int r;

    if (!s || !s->impl)
        return -EINVAL;

    r = gxfp_cmd_read_otp(&SESSION(s)->dev, otp, (uint16_t)sizeof(otp), &otp_len);
    if (r < 0)
        return r;

    r = gxfp_milanl_parse_otp(otp, (size_t)otp_len, &otp_cfg);
    if (r < 0)
        return r;

    r = gxfp_milanl_get_default_cfg_template(&template_data, &template_len);
    if (r < 0)
        return r;

    cfg_blob_len = gxfp_milanl_prepare_cfg_blob(template_data,
                                                template_len,
                                                cfg_blob,
                                                sizeof(cfg_blob));
    if (cfg_blob_len < 0)
        return cfg_blob_len;

    r = gxfp_milanl_apply_otp_patch(cfg_blob,
                                    (size_t)cfg_blob_len,
                                    &otp_cfg);
    if (r < 0)
        return r;

    r = gxfp_cmd_upload_config_mcu(&SESSION(s)->dev,
                                   cfg_blob,
                                   (uint16_t)cfg_blob_len,
                                   &ack_status);
    if (r < 0)
        return r;

    SESSION(s)->otp_ctx.tcode = otp_cfg.tcode;
    SESSION(s)->otp_ctx.fdt_delta = otp_cfg.fdt_delta;
    SESSION(s)->otp_ctx.has_tcode_delta = otp_cfg.has_tcode_delta;
    session_apply_fdt_runtime_from_otp(s);

    return 0;
}

static void
session_set_error(struct gxfp_session *s,
                  struct gxfp_session_events *ev,
                  int err,
                  enum gxfp_session_error_target target,
                  const char *fmt,
                  ...)
{
    va_list ap;

    if (!ev)
        return;

    ev->session_error = 1;
    ev->error_code = err;
    ev->error_target = target;

    va_start(ap, fmt);
    vsnprintf(ev->error_msg, sizeof(ev->error_msg), fmt, ap);
    va_end(ap);

    if (s && s->impl && err != -ECANCELED &&
        (target == GXFP_SESSION_ERR_TARGET_GENERIC || target == GXFP_SESSION_ERR_TARGET_ACTIVATE) &&
        gxfp_session_mode_has_io(s)) {
        (void)gxfp_device_recovery(&SESSION(s)->dev, 1, 1, 0);
    }

    if (s && s->impl)
        SESSION(s)->mode = GXFP_MODE_FAILED;
}

static int
gxfp_session_mode_has_io(const struct gxfp_session *s)
{
    return s && s->impl && SESSION(s)->mode != GXFP_MODE_CLOSED;
}

static void
gxfp_session_stop_run(struct gxfp_session *s)
{
    if (!s || !s->impl)
        return;

    if (SESSION(s)->mode != GXFP_MODE_CLOSED)
        SESSION(s)->mode = GXFP_MODE_READY;
}

static void
session_reset_buffers(struct gxfp_session *s)
{
    if (SESSION(s)->dark_loaded) {
        gxfp_fpn_free(&SESSION(s)->dark);
        SESSION(s)->dark_loaded = 0;
    }

    if (SESSION(s)->pending_img_ready) {
        gxfp_decoded_image_free(&SESSION(s)->pending_img);
        SESSION(s)->pending_img_ready = 0;
    }
}

static void
gxfp_session_reset_runtime(struct gxfp_session *s)
{
    if (!s || !s->impl)
        return;

    if (gxfp_session_mode_has_io(s)) {
        gxfp_tls_service_free(&SESSION(s)->svc);
        gxfp_dev_close(&SESSION(s)->dev);
    }

    memset(&SESSION(s)->dev, 0, sizeof(SESSION(s)->dev));
    memset(&SESSION(s)->svc, 0, sizeof(SESSION(s)->svc));
    gxfp_fdt_flow_init(&SESSION(s)->fdt);
    SESSION(s)->otp_ctx.tcode = 0;
    SESSION(s)->otp_ctx.fdt_delta = 0;
    SESSION(s)->otp_ctx.has_tcode_delta = 0;
    session_apply_fdt_runtime_from_otp(s);

    free(SESSION(s)->psk);
    SESSION(s)->psk = NULL;
    SESSION(s)->psk_len = 0;

    session_reset_buffers(s);
    SESSION(s)->mode = GXFP_MODE_CLOSED;
}

static void
session_apply_fdt_runtime_from_otp(struct gxfp_session *s)
{
    int16_t fdt_delta = 0;

    if (!s || !s->impl)
        return;

    if (SESSION(s)->otp_ctx.has_tcode_delta)
        fdt_delta = (int16_t)SESSION(s)->otp_ctx.fdt_delta;

    gxfp_cmd_fdt_state_set_runtime(&SESSION(s)->fdt.cmd,
                                   0,
                                   0,
                                   0,
                                   fdt_delta);
}

static void
session_begin_cleanup(struct gxfp_session *s,
                      enum gxfp_cleanup_goal goal,
                      int reset_mcu,
                      struct gxfp_session_events *ev)
{
    if (!s || !s->impl || !ev)
        return;

    gxfp_session_stop_run(s);
    ev->cancel_tick = 1;

    if (!gxfp_session_mode_has_io(s)) {
        if (goal == GXFP_CLEANUP_CLOSE)
            ev->close_complete = 1;
        else
            ev->deactivate_complete = 1;
        return;
    }

    if (goal == GXFP_CLEANUP_DEACTIVATE) {
        SESSION(s)->mode = GXFP_MODE_READY;
        ev->deactivate_complete = 1;
        return;
    }

    if (reset_mcu)
        (void)gxfp_device_recovery(&SESSION(s)->dev, 1, 1, 0);

    gxfp_session_reset_runtime(s);
    ev->close_complete = 1;
}

static int
session_ensure_tap_io_buffer(struct gxfp_session *s)
{
    size_t cap;

    if (!s || !s->impl)
        return -EINVAL;
    if (SESSION(s)->tap_io_buf)
        return 0;

    cap = sizeof(struct gxfp_tap_hdr) + (size_t)GXFP_IOCTL_TAP_PAYLOAD_MAX;
    SESSION(s)->tap_io_buf = (uint8_t *)malloc(cap);
    if (!SESSION(s)->tap_io_buf)
        return -ENOMEM;

    SESSION(s)->tap_io_cap = cap;
    return 0;
}

static void
session_set_fdt_mode(struct gxfp_session *s,
                     struct gxfp_session_events *ev,
                     enum gxfp_fdt_mode mode)
{
    int r;

    if (!gxfp_session_mode_has_io(s) || !ev)
        return;

    r = gxfp_fdt_flow_set_mode(&SESSION(s)->fdt, &SESSION(s)->dev, mode);
    if (r < 0)
        session_set_error(s,
                          ev,
                          r,
                          GXFP_SESSION_ERR_TARGET_GENERIC,
                          "fdt_set_mode(%d) failed: %s",
                          (int)mode,
                          strerror(-r));
}

static int
session_tls_init(struct gxfp_session *s, int enable_log)
{
    struct gxfp_tls_hs_cfg cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.timeout_ms = 20000;
    cfg.manual_pending_request = 1;
    cfg.logf = enable_log ? stderr : NULL;

    return gxfp_tls_service_init(&SESSION(s)->svc, &SESSION(s)->dev, &cfg, SESSION(s)->psk, SESSION(s)->psk_len);
}

static void
session_rearm_tls_if_needed(struct gxfp_session *s, int enable_log, struct gxfp_session_events *ev)
{
    enum gxfp_tls_hs_state st;
    int r;

    st = gxfp_tls_service_state(&SESSION(s)->svc);
    if (st == GXFP_TLS_HS_INIT || st == GXFP_TLS_HS_CONNECTING)
        return;

    if (st == GXFP_TLS_HS_CONNECTED || st == GXFP_TLS_HS_CAPTURING || st == GXFP_TLS_HS_DONE)
        return;

    gxfp_tls_service_free(&SESSION(s)->svc);
    (void)gxfp_dev_flush_rxq(&SESSION(s)->dev);

    r = session_tls_init(s, enable_log);
    if (r < 0)
        session_set_error(s,
                          ev,
                          r,
                          GXFP_SESSION_ERR_TARGET_ACTIVATE,
                          "tls_service init failed: %s",
                          strerror(-r));
}

static int
session_set_dark_from_image(struct gxfp_session *s, const struct gxfp_decoded_image *img)
{
    size_t count;
    uint16_t *pixels;

    if (!s || !s->impl || !img || !img->pixels || img->rows <= 0 || img->cols <= 0)
        return -EINVAL;

    count = (size_t)img->rows * (size_t)img->cols;
    pixels = (uint16_t *)malloc(count * sizeof(uint16_t));
    if (!pixels)
        return -ENOMEM;

    memcpy(pixels, img->pixels, count * sizeof(uint16_t));
    if (SESSION(s)->dark_loaded)
        gxfp_fpn_free(&SESSION(s)->dark);

    SESSION(s)->dark.pixels = pixels;
    SESSION(s)->dark.rows = img->rows;
    SESSION(s)->dark.cols = img->cols;
    SESSION(s)->dark_loaded = 1;
    return 0;
}

void
gxfp_session_events_clear(struct gxfp_session_events *ev)
{
    if (!ev)
        return;

    memset(ev, 0, sizeof(*ev));
    ev->error_target = GXFP_SESSION_ERR_TARGET_GENERIC;
}

void
gxfp_session_init(struct gxfp_session *s)
{
    int r;

    if (!s)
        return;

    memset(s, 0, sizeof(*s));
    r = session_ensure_impl(s);
    (void)r;
}

void
gxfp_session_dispose(struct gxfp_session *s)
{
    if (!s || !s->impl)
        return;

    gxfp_session_reset_runtime(s);

    free(SESSION(s)->tap_io_buf);
    SESSION(s)->tap_io_buf = NULL;
    SESSION(s)->tap_io_cap = 0;

    free(s->impl);
    s->impl = NULL;
}

int
gxfp_session_open(struct gxfp_session *s,
                  const char *dev_path,
                  const uint8_t *psk,
                  size_t psk_len,
                  int enable_log,
                  char *errbuf,
                  size_t errbuf_len)
{
    int r;

    if (!s || !dev_path || !psk || psk_len == 0)
        return -EINVAL;

    r = session_ensure_impl(s);
    if (r < 0)
        return r;

    gxfp_session_reset_runtime(s);

    r = gxfp_dev_open(&SESSION(s)->dev, dev_path, O_RDWR | O_NONBLOCK);
    if (r < 0) {
        if (errbuf && errbuf_len)
            snprintf(errbuf, errbuf_len, "open(%s) failed: %s", dev_path, strerror(-r));
        return r;
    }

    SESSION(s)->mode = GXFP_MODE_READY;

    r = session_ensure_tap_io_buffer(s);
    if (r < 0) {
        if (errbuf && errbuf_len)
            snprintf(errbuf, errbuf_len, "failed to allocate tap io buffer");
        gxfp_session_reset_runtime(s);
        return r;
    }

    (void)gxfp_dev_flush_rxq(&SESSION(s)->dev);

    uint16_t chip_id = 0;
    r = gxfp_cmd_read_chip_id(&SESSION(s)->dev, &chip_id);
    if (r == 0 && chip_id == 0x2208) {
        r = session_apply_milanl_cfg(s);
        if (r < 0) {
            if (errbuf && errbuf_len)
                snprintf(errbuf,
                         errbuf_len,
                         "otp/config bootstrap failed: %s",
                         strerror(-r));
            gxfp_session_reset_runtime(s);
            return r;
        }
    }

    SESSION(s)->psk = (uint8_t *)malloc(psk_len);
    if (!SESSION(s)->psk) {
        gxfp_session_reset_runtime(s);
        return -ENOMEM;
    }

    memcpy(SESSION(s)->psk, psk, psk_len);
    SESSION(s)->psk_len = psk_len;

    r = session_tls_init(s, enable_log);
    if (r < 0) {
        if (errbuf && errbuf_len)
            snprintf(errbuf, errbuf_len, "tls_service_init failed: %s", strerror(-r));
        gxfp_session_reset_runtime(s);
        return r;
    }

    gxfp_fdt_flow_init(&SESSION(s)->fdt);
    session_apply_fdt_runtime_from_otp(s);
    SESSION(s)->mode = GXFP_MODE_READY;
    return 0;
}

void
gxfp_session_request_close(struct gxfp_session *s,
                           int reset_mcu,
                           struct gxfp_session_events *ev)
{
    session_begin_cleanup(s, GXFP_CLEANUP_CLOSE, reset_mcu, ev);
}

void
gxfp_session_request_deactivate(struct gxfp_session *s,
                                struct gxfp_session_events *ev)
{
    session_begin_cleanup(s, GXFP_CLEANUP_DEACTIVATE, 0, ev);
}

int
gxfp_session_activate(struct gxfp_session *s,
                      int enable_log,
                      struct gxfp_session_events *ev)
{
    if (!s || !s->impl || !ev)
        return -EINVAL;
    if (!gxfp_session_mode_has_io(s))
        return -EINVAL;

    if (SESSION(s)->mode == GXFP_MODE_FAILED)
        SESSION(s)->mode = GXFP_MODE_READY;

    session_rearm_tls_if_needed(s, enable_log, ev);
    if (ev->session_error)
        return ev->error_code ? ev->error_code : -EIO;

    gxfp_fdt_flow_init(&SESSION(s)->fdt);
    session_apply_fdt_runtime_from_otp(s);
    SESSION(s)->mode = GXFP_MODE_ACTIVATING_TLS;

    ev->request_tick = 1;
    ev->tick_delay_ms = 0;
    return 0;
}

static void
session_maybe_finish_activation(struct gxfp_session *s, struct gxfp_session_events *ev)
{
    enum gxfp_tls_hs_state st;

    if (!s || !s->impl || SESSION(s)->mode != GXFP_MODE_ACTIVATING_TLS)
        return;

    st = gxfp_tls_service_state(&SESSION(s)->svc);
    if (st != GXFP_TLS_HS_CONNECTED && st != GXFP_TLS_HS_DONE)
        return;

    session_set_fdt_mode(s, ev, GXFP_FDT_MODE_WAIT_UP);
    if (ev->session_error)
        return;

    SESSION(s)->mode = GXFP_MODE_ACTIVATING_WAIT_UP;
    ev->request_tick = 1;
    ev->tick_delay_ms = 0;
}

static void
session_finalize_capture(struct gxfp_session *s,
                         struct gxfp_session_events *ev,
                         enum gxfp_session_error_target target)
{
    uint8_t hdr[5];
    uint8_t *body = NULL;
    size_t body_len = 0;
    struct gxfp_decoded_image img;
    int r;

    if (!s || !s->impl)
        return;
    if (gxfp_tls_service_state(&SESSION(s)->svc) != GXFP_TLS_HS_DONE)
        return;

    r = gxfp_tls_service_get_raw_image(&SESSION(s)->svc, hdr, &body, &body_len);
    if (r <= 0) {
        session_set_error(s,
                          ev,
                          (r < 0) ? r : -EIO,
                          target,
                          "capture finished but no image (r=%d)",
                          r);
        return;
    }

    memset(&img, 0, sizeof(img));
    r = gxfp_decode_image(body, body_len, GXFP_DECODE_AUTO, &img);
    free(body);
    if (r < 0) {
        session_set_error(s, ev, r, target, "decode failed: %s", strerror(-r));
        return;
    }

    if (SESSION(s)->mode == GXFP_MODE_ACTIVATING_DARK_CAPTURE) {
        r = session_set_dark_from_image(s, &img);
        gxfp_decoded_image_free(&img);
        if (r < 0) {
            session_set_error(s, ev, r, GXFP_SESSION_ERR_TARGET_ACTIVATE, "dark frame failed: %s", strerror(-r));
            return;
        }

        session_set_fdt_mode(s, ev, GXFP_FDT_MODE_IDLE);
        if (ev->session_error)
            return;

        SESSION(s)->mode = GXFP_MODE_IDLE;
        ev->activate_complete = 1;
        return;
    }

    if (SESSION(s)->dark_loaded) {
        r = gxfp_fpn_correct(img.pixels, img.rows, img.cols, &SESSION(s)->dark);
        if (r < 0) {
            gxfp_decoded_image_free(&img);
            session_set_error(s, ev, r, GXFP_SESSION_ERR_TARGET_CAPTURE, "fpn correct failed: %s", strerror(-r));
            return;
        }
    }

    if (SESSION(s)->pending_img_ready)
        gxfp_decoded_image_free(&SESSION(s)->pending_img);

    SESSION(s)->pending_img = img;
    SESSION(s)->pending_img_ready = 1;
    SESSION(s)->mode = GXFP_MODE_IDLE;
    ev->image_ready = 1;
}

static void
session_poll_tls(struct gxfp_session *s, struct gxfp_session_events *ev)
{
    int r;
    int i;

    for (i = 0; i < 64; i++) {
        r = gxfp_tls_service_step(&SESSION(s)->svc);
        if (r == -EAGAIN || r == 1)
            break;
        if (r < 0) {
            char tls_err[128];
            session_set_error(s,
                              ev,
                              r,
                              GXFP_SESSION_ERR_TARGET_GENERIC,
                              "step failed: %s",
                              gxfp_tls_service_strerror(r, tls_err, sizeof(tls_err)));
            return;
        }
        if (i == 63) {
            ev->request_tick = 1;
            ev->tick_delay_ms = 0;
        }
    }
}

void
gxfp_session_pump(struct gxfp_session *s,
                  int action_cancelled,
                  struct gxfp_session_events *ev)
{
    enum gxfp_tls_hs_state st;
    int r;

    if (!s || !s->impl || !ev)
        return;

    if (SESSION(s)->mode == GXFP_MODE_CLOSED || SESSION(s)->mode == GXFP_MODE_READY ||
        SESSION(s)->mode == GXFP_MODE_FAILED)
        return;

    if (action_cancelled && (SESSION(s)->mode == GXFP_MODE_ACTIVATING_TLS ||
                             SESSION(s)->mode == GXFP_MODE_ACTIVATING_WAIT_UP ||
                             SESSION(s)->mode == GXFP_MODE_ACTIVATING_DARK_CAPTURE ||
                             SESSION(s)->mode == GXFP_MODE_CAPTURING)) {
        enum gxfp_session_error_target target = GXFP_SESSION_ERR_TARGET_CAPTURE;
        if (SESSION(s)->mode != GXFP_MODE_CAPTURING)
            target = GXFP_SESSION_ERR_TARGET_ACTIVATE;
        session_set_error(s, ev, -ECANCELED, target, "Cancelled");
        ev->cancel_tick = 1;
        return;
    }

    session_poll_tls(s, ev);
    if (ev->session_error)
        return;

    session_maybe_finish_activation(s, ev);
    if (ev->session_error)
        return;

    if (SESSION(s)->mode == GXFP_MODE_WAIT_FINGER_OFF ||
        SESSION(s)->mode == GXFP_MODE_ACTIVATING_WAIT_UP) {
        r = gxfp_fdt_flow_poll(&SESSION(s)->fdt,
                               &SESSION(s)->dev,
                               gxfp_monotonic_ms());
        if (r < 0) {
            session_set_error(s, ev, r, GXFP_SESSION_ERR_TARGET_GENERIC,
                              "fdt wait-up retry failed: %s", strerror(-r));
            return;
        }
    }

    if (SESSION(s)->mode == GXFP_MODE_ACTIVATING_DARK_CAPTURE || SESSION(s)->mode == GXFP_MODE_CAPTURING)
        session_finalize_capture(s,
                                 ev,
                                 SESSION(s)->mode == GXFP_MODE_CAPTURING ? GXFP_SESSION_ERR_TARGET_CAPTURE :
                                                                            GXFP_SESSION_ERR_TARGET_ACTIVATE);

    st = gxfp_tls_service_state(&SESSION(s)->svc);
    if (SESSION(s)->mode == GXFP_MODE_ACTIVATING_TLS && st != GXFP_TLS_HS_CONNECTED && st != GXFP_TLS_HS_ERROR) {
        ev->request_tick = 1;
        ev->tick_delay_ms = 20;
    }
    if ((SESSION(s)->mode == GXFP_MODE_ACTIVATING_DARK_CAPTURE || SESSION(s)->mode == GXFP_MODE_CAPTURING) &&
        st != GXFP_TLS_HS_DONE && st != GXFP_TLS_HS_ERROR) {
        ev->request_tick = 1;
        ev->tick_delay_ms = 20;
    }
    if (SESSION(s)->mode == GXFP_MODE_WAIT_FINGER_ON || SESSION(s)->mode == GXFP_MODE_WAIT_FINGER_OFF ||
        SESSION(s)->mode == GXFP_MODE_ACTIVATING_WAIT_UP) {
        ev->request_tick = 1;
        ev->tick_delay_ms = 20;
    }
}

void
gxfp_session_on_fd(struct gxfp_session *s,
                   uint32_t condition,
                   int action_cancelled,
                   struct gxfp_session_events *ev)
{
    if (!s || !s->impl || !ev)
        return;
    if (SESSION(s)->mode == GXFP_MODE_CLOSED || SESSION(s)->mode == GXFP_MODE_READY ||
        SESSION(s)->mode == GXFP_MODE_FAILED)
        return;

    if (condition & (GXFP_SESSION_IO_ERR | GXFP_SESSION_IO_HUP | GXFP_SESSION_IO_NVAL)) {
        session_set_error(s, ev, -EIO, GXFP_SESSION_ERR_TARGET_GENERIC, "gxfp fd error/hup");
        return;
    }

    for (;;) {
        uint8_t *buf;
        size_t buf_cap;
        ssize_t n;
        struct gxfp_tap_hdr tap_hdr;
        const uint8_t *tap_payload;
        uint32_t fdt_events = GXFP_FDT_EVENT_NONE;
        int r;

        if (!SESSION(s)->tap_io_buf || SESSION(s)->tap_io_cap == 0) {
            session_set_error(s, ev, -ENOMEM, GXFP_SESSION_ERR_TARGET_GENERIC, "gxfp io buffer unavailable");
            break;
        }

        buf = SESSION(s)->tap_io_buf;
        buf_cap = SESSION(s)->tap_io_cap;
        n = gxfp_dev_read_record(&SESSION(s)->dev, buf, buf_cap, &tap_hdr, &tap_payload, NULL);
        if (n < 0) {
            if ((int)n == -EAGAIN)
                break;
            session_set_error(s, ev, (int)n, GXFP_SESSION_ERR_TARGET_GENERIC, "gxfp read failed: %s", strerror((int)-n));
            break;
        }
        if (n == 0)
            break;

        r = gxfp_fdt_flow_feed_record(&SESSION(s)->fdt,
                                      &SESSION(s)->dev,
                                      &tap_hdr,
                                      tap_payload,
                                      (size_t)tap_hdr.len,
                                      &fdt_events);
        if (r < 0 && r != -EAGAIN) {
            session_set_error(s, ev, r, GXFP_SESSION_ERR_TARGET_GENERIC, "fdt_feed_record failed: %s", strerror(-r));
            break;
        }

        if (SESSION(s)->mode == GXFP_MODE_WAIT_FINGER_ON && (fdt_events & GXFP_FDT_EVENT_DOWN)) {
            ev->finger_status_changed = 1;
            ev->finger_present = 1;
        }

        if (SESSION(s)->mode == GXFP_MODE_WAIT_FINGER_OFF && (fdt_events & GXFP_FDT_EVENT_UP)) {
            ev->finger_status_changed = 1;
            ev->finger_present = 0;
        }

        if (SESSION(s)->mode == GXFP_MODE_ACTIVATING_WAIT_UP && (fdt_events & GXFP_FDT_EVENT_UP)) {
            r = gxfp_tls_service_request_capture(&SESSION(s)->svc);
            if (r < 0) {
                session_set_error(s,
                                  ev,
                                  r,
                                  GXFP_SESSION_ERR_TARGET_ACTIVATE,
                                  "dark frame request_capture failed: %s",
                                  strerror(-r));
                break;
            }

            SESSION(s)->mode = GXFP_MODE_ACTIVATING_DARK_CAPTURE;
            ev->request_tick = 1;
            ev->tick_delay_ms = 0;
        }

        r = gxfp_tls_service_feed_tap_record(&SESSION(s)->svc, &tap_hdr, tap_payload, (size_t)tap_hdr.len);
        if (r < 0) {
            session_set_error(s, ev, r, GXFP_SESSION_ERR_TARGET_GENERIC, "gxfp feed failed: %s", strerror(-r));
            break;
        }
    }

    gxfp_session_pump(s, action_cancelled, ev);
}

void
gxfp_session_change_state(struct gxfp_session *s,
                          enum gxfp_session_state state,
                          struct gxfp_session_events *ev)
{
    int r;

    if (!s || !s->impl || !ev)
        return;
    if (!gxfp_session_mode_has_io(s) || SESSION(s)->mode == GXFP_MODE_FAILED)
        return;

    switch (state) {
    case GXFP_SESSION_STATE_AWAIT_FINGER_ON:
        SESSION(s)->mode = GXFP_MODE_WAIT_FINGER_ON;
        session_set_fdt_mode(s, ev, GXFP_FDT_MODE_WAIT_DOWN);
        if (!ev->session_error) {
            ev->request_tick = 1;
            ev->tick_delay_ms = 0;
        }
        break;

    case GXFP_SESSION_STATE_CAPTURE:
        if (SESSION(s)->mode != GXFP_MODE_IDLE && SESSION(s)->mode != GXFP_MODE_WAIT_FINGER_ON) {
            session_set_error(s, ev, -EINVAL, GXFP_SESSION_ERR_TARGET_CAPTURE, "capture state transition rejected");
            break;
        }

        r = gxfp_tls_service_request_capture(&SESSION(s)->svc);
        if (r < 0) {
            session_set_error(s, ev, r, GXFP_SESSION_ERR_TARGET_CAPTURE, "request_capture failed: %s", strerror(-r));
            break;
        }

        SESSION(s)->mode = GXFP_MODE_CAPTURING;
        ev->request_tick = 1;
        ev->tick_delay_ms = 0;
        break;

    case GXFP_SESSION_STATE_AWAIT_FINGER_OFF:
        SESSION(s)->mode = GXFP_MODE_WAIT_FINGER_OFF;
        session_set_fdt_mode(s, ev, GXFP_FDT_MODE_WAIT_UP);
        if (!ev->session_error) {
            ev->request_tick = 1;
            ev->tick_delay_ms = 0;
        }
        break;

    case GXFP_SESSION_STATE_IDLE:
        SESSION(s)->mode = GXFP_MODE_IDLE;
        session_set_fdt_mode(s, ev, GXFP_FDT_MODE_IDLE);
        break;

    case GXFP_SESSION_STATE_INACTIVE:
    default:
        gxfp_session_stop_run(s);
        session_set_fdt_mode(s, ev, GXFP_FDT_MODE_IDLE);
        break;
    }
}

int
gxfp_session_take_image(struct gxfp_session *s, struct gxfp_decoded_image *out)
{
    if (!s || !s->impl || !out)
        return -EINVAL;
    if (!SESSION(s)->pending_img_ready)
        return -ENOENT;

    *out = SESSION(s)->pending_img;
    memset(&SESSION(s)->pending_img, 0, sizeof(SESSION(s)->pending_img));
    SESSION(s)->pending_img_ready = 0;
    return 0;
}

int
gxfp_session_poll_readable(const struct gxfp_session *s, int timeout_ms)
{
    if (!gxfp_session_mode_has_io(s))
        return -EINVAL;
    return gxfp_dev_poll_readable(&SESSION(s)->dev, timeout_ms);
}
