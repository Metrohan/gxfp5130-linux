#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gxfp/algo/image/decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

enum gxfp_session_state {
    GXFP_SESSION_STATE_INACTIVE = 0,
    GXFP_SESSION_STATE_IDLE,
    GXFP_SESSION_STATE_AWAIT_FINGER_ON,
    GXFP_SESSION_STATE_CAPTURE,
    GXFP_SESSION_STATE_AWAIT_FINGER_OFF,
};

enum gxfp_session_error_target {
    GXFP_SESSION_ERR_TARGET_GENERIC = 0,
    GXFP_SESSION_ERR_TARGET_ACTIVATE,
    GXFP_SESSION_ERR_TARGET_CAPTURE,
};

struct gxfp_session_events {
    int activate_complete;
    int deactivate_complete;
    int close_complete;
    int session_error;
    int finger_status_changed;
    int finger_present;
    int image_ready;

    int request_tick;
    int tick_delay_ms;
    int cancel_tick;

    int error_code;
    enum gxfp_session_error_target error_target;
    char error_msg[192];
};

struct gxfp_session {
    void *impl;
};

enum gxfp_session_io_condition {
    GXFP_SESSION_IO_IN = 1u << 0,
    GXFP_SESSION_IO_ERR = 1u << 3,
    GXFP_SESSION_IO_HUP = 1u << 4,
    GXFP_SESSION_IO_NVAL = 1u << 5,
};

void gxfp_session_events_clear(struct gxfp_session_events *ev);

void gxfp_session_init(struct gxfp_session *s);
void gxfp_session_dispose(struct gxfp_session *s);

int gxfp_session_open(struct gxfp_session *s,
                      const char *dev_path,
                      const uint8_t *psk,
                      size_t psk_len,
                      int enable_log,
                      char *errbuf,
                      size_t errbuf_len);

void gxfp_session_request_close(struct gxfp_session *s,
                                int reset_mcu,
                                struct gxfp_session_events *ev);

void gxfp_session_request_deactivate(struct gxfp_session *s,
                                     struct gxfp_session_events *ev);

int gxfp_session_activate(struct gxfp_session *s,
                          int enable_log,
                          struct gxfp_session_events *ev);

void gxfp_session_change_state(struct gxfp_session *s,
                               enum gxfp_session_state state,
                               struct gxfp_session_events *ev);

void gxfp_session_on_fd(struct gxfp_session *s,
                        uint32_t condition,
                        int action_cancelled,
                        struct gxfp_session_events *ev);

void gxfp_session_pump(struct gxfp_session *s,
                       int action_cancelled,
                       struct gxfp_session_events *ev);

int gxfp_session_take_image(struct gxfp_session *s, struct gxfp_decoded_image *out);
int gxfp_session_poll_readable(const struct gxfp_session *s, int timeout_ms);

#ifdef __cplusplus
}
#endif
