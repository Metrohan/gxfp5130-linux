#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "gxfp/io/dev.h"
#include "gxfp/io/uapi.h"

#ifdef __cplusplus
extern "C" {
#endif

enum gxfp_tls_hs_state {
	GXFP_TLS_HS_INIT = 0,
	GXFP_TLS_HS_CONNECTING,
	GXFP_TLS_HS_CONNECTED,
	GXFP_TLS_HS_CAPTURING,
	GXFP_TLS_HS_DONE,
	GXFP_TLS_HS_ERROR,
};

struct gxfp_tls_hs_cfg {
	uint32_t timeout_ms;
	int manual_pending_request;
	FILE *logf;
};

struct gxfp_tls_service {
	void *impl;
};

int gxfp_tls_service_init(struct gxfp_tls_service *svc,
			  struct gxfp_dev *dev,
			  const struct gxfp_tls_hs_cfg *cfg,
			  const uint8_t *psk,
			  size_t psk_len);

void gxfp_tls_service_free(struct gxfp_tls_service *svc);

int gxfp_tls_service_feed_tap_record(struct gxfp_tls_service *svc,
				     const struct gxfp_tap_hdr *hdr,
				     const uint8_t *payload,
				     size_t payload_len);

int gxfp_tls_service_step(struct gxfp_tls_service *svc);
int gxfp_tls_service_request_capture(struct gxfp_tls_service *svc);

int gxfp_tls_service_get_raw_image(struct gxfp_tls_service *svc,
				   uint8_t out_hdr[5],
				   uint8_t **out_body,
				   size_t *out_body_len);

const char *gxfp_tls_service_strerror(int err, char *buf, size_t buf_len);

enum gxfp_tls_hs_state gxfp_tls_service_state(const struct gxfp_tls_service *svc);
int gxfp_tls_service_error(const struct gxfp_tls_service *svc);

#ifdef __cplusplus
}
#endif
