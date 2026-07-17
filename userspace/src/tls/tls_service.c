#include "gxfp/tls/tls_service.h"

#include "../tls/tls_engine.h"

#include "gxfp/cmd/tls_cmd.h"
#include "gxfp/algo/common.h"
#include "gxfp/algo/image/plain.h"
#include "gxfp/proto/goodix_constants.h"
#include "gxfp/tls/tls_constants.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct gxfp_tls_service_impl {
	struct gxfp_dev *dev;
	struct gxfp_tls_hs_cfg cfg;
	enum gxfp_tls_hs_state state;
	int err;
	int pending_capture;
	int capture_sent;
	int64_t handshake_start_ms;
	int64_t last_plain_ms;
	int64_t last_tls_rx_ms;
	struct gxfp_tls_engine *engine;
	struct gxfp_plain_image_sink img;
};

static void svc_mark_capture_started(struct gxfp_tls_service_impl *s)
{
	if (!s)
		return;

	int64_t now = gxfp_monotonic_ms();

	s->capture_sent = 1;
	s->pending_capture = 0;
	s->state = GXFP_TLS_HS_CAPTURING;
	s->last_plain_ms = now;
	s->last_tls_rx_ms = now;
}

static int svc_capture_timed_out(struct gxfp_tls_service_impl *s)
{
	if (!s)
		return 0;

	int64_t now = gxfp_monotonic_ms();

	return (uint32_t)(now - s->last_plain_ms) > s->cfg.timeout_ms &&
	       (uint32_t)(now - s->last_tls_rx_ms) > s->cfg.timeout_ms;
}

static void svc_impl_free(struct gxfp_tls_service_impl *s)
{
	if (!s)
		return;
	gxfp_tls_engine_free(s->engine);
	gxfp_plain_image_sink_reset(&s->img);
	free(s);
}

static int send_tls_record_cb(void *ctx, const uint8_t *rec, size_t rec_len)
{
	struct gxfp_tls_service_impl *s = (struct gxfp_tls_service_impl *)ctx;
	if (!s || !s->dev || !rec || rec_len == 0 || rec_len > 0xFFFFu)
		return -EINVAL;
	return gxfp_dev_send_packet(s->dev, GOODIX_MP_TYPE_TLS, rec, (uint16_t)rec_len);
}

int gxfp_tls_service_init(struct gxfp_tls_service *svc,
			  struct gxfp_dev *dev,
			  const struct gxfp_tls_hs_cfg *cfg,
			  const uint8_t *psk,
			  size_t psk_len)
{
	struct gxfp_tls_service_impl *s;
	int r;

	if (!svc || !dev || !psk || psk_len != GXFP5130_PSK_LEN)
		return -EINVAL;
	if (svc->impl)
		return -EALREADY;

	s = (struct gxfp_tls_service_impl *)calloc(1, sizeof(*s));
	if (!s)
		return -ENOMEM;

	s->dev = dev;
	s->state = GXFP_TLS_HS_INIT;
	s->handshake_start_ms = gxfp_monotonic_ms();
	s->last_plain_ms = s->handshake_start_ms;
	s->last_tls_rx_ms = s->handshake_start_ms;

	if (cfg)
		s->cfg = *cfg;

	gxfp_plain_image_sink_reset(&s->img);

	r = gxfp_tls_engine_create(&s->engine,
				   psk,
				   psk_len,
				   send_tls_record_cb,
				   s);
	if (r < 0)
		goto fail;

	(void)gxfp_dev_set_nonblock(dev, 1);
	(void)gxfp_dev_flush_rxq(dev);

	r = gxfp_cmd_tls_server_init(s->dev);
	if (r < 0)
		goto fail;

	s->state = GXFP_TLS_HS_CONNECTING;
	s->handshake_start_ms = gxfp_monotonic_ms();
	svc->impl = s;
	return 0;

fail:
	svc_impl_free(s);
	return r;
}

void gxfp_tls_service_free(struct gxfp_tls_service *svc)
{
	struct gxfp_tls_service_impl *s;

	if (!svc)
		return;
	s = (struct gxfp_tls_service_impl *)svc->impl;
	if (!s)
		return;

	svc_impl_free(s);
	svc->impl = NULL;
}

int gxfp_tls_service_feed_tap_record(struct gxfp_tls_service *svc,
				     const struct gxfp_tap_hdr *hdr,
				     const uint8_t *payload,
				     size_t payload_len)
{
	struct gxfp_tls_service_impl *s;

	if (!svc || !svc->impl)
		return -EINVAL;
	s = (struct gxfp_tls_service_impl *)svc->impl;

	if (hdr->type == GOODIX_MP_TYPE_TLS) {
		int r;

		s->last_tls_rx_ms = gxfp_monotonic_ms();
		r = gxfp_tls_engine_feed_cipher_record(s->engine, payload, payload_len);
		if (r < 0) {
			s->err = r;
			s->state = GXFP_TLS_HS_ERROR;
			return r;
		}
	}

	return 0;
}

int gxfp_tls_service_request_capture(struct gxfp_tls_service *svc)
{
	struct gxfp_tls_service_impl *s;

	if (!svc || !svc->impl)
		return -EINVAL;
	s = (struct gxfp_tls_service_impl *)svc->impl;

	if (s->state != GXFP_TLS_HS_CONNECTED && s->state != GXFP_TLS_HS_DONE)
		return -EBUSY;

	s->pending_capture = 1;
	s->capture_sent = 0;
	gxfp_plain_image_sink_reset(&s->img);
	s->state = GXFP_TLS_HS_CONNECTED;
	return 0;
}

int gxfp_tls_service_step(struct gxfp_tls_service *svc)
{
	struct gxfp_tls_service_impl *s;
	int r;
	int progressed = 0;

	if (!svc || !svc->impl)
		return -EINVAL;
	s = (struct gxfp_tls_service_impl *)svc->impl;

	if (s->state == GXFP_TLS_HS_ERROR)
		return s->err ? s->err : -EIO;

	if (s->state == GXFP_TLS_HS_CONNECTING) {
		r = gxfp_tls_engine_handshake_step(s->engine);
		if (r == 1) {
			s->state = GXFP_TLS_HS_CONNECTED;
			if (!s->cfg.manual_pending_request)
				s->pending_capture = 1;
			return 0;
		}
		if (r == -EAGAIN) {
			if ((uint32_t)(gxfp_monotonic_ms() - s->handshake_start_ms) > s->cfg.timeout_ms)
				r = -ETIMEDOUT;
			else
				return -EAGAIN;
		}
		s->err = r;
		s->state = GXFP_TLS_HS_ERROR;
		return r;
	}

	if (s->state == GXFP_TLS_HS_CONNECTED && s->pending_capture && !s->capture_sent) {
		r = gxfp_cmd_get_image(s->dev);
		if (r < 0) {
			s->err = r;
			s->state = GXFP_TLS_HS_ERROR;
			return r;
		}
		svc_mark_capture_started(s);
	}

	if (s->state == GXFP_TLS_HS_CAPTURING) {
		for (;;) {
			uint8_t plain[TLS_REC_MAX_PLAIN];
			size_t plain_len = 0;
			r = gxfp_tls_engine_pull_plain(s->engine, plain, sizeof(plain), &plain_len);
			if (r == 1) {
				gxfp_plain_scan_for_nonpov_image_chunks(plain, plain_len, &s->img);
				s->last_plain_ms = gxfp_monotonic_ms();
				progressed = 1;
				if (s->img.have_oneframe) {
					s->state = GXFP_TLS_HS_DONE;
					return 1;
				}
				continue;
			}
			if (r == -EAGAIN)
				break;
			s->err = r;
			s->state = GXFP_TLS_HS_ERROR;
			return r;
		}

		if (!progressed) {
			if (svc_capture_timed_out(s)) {
				s->err = -ETIMEDOUT;
				s->state = GXFP_TLS_HS_ERROR;
				return s->err;
			}
		}
		return progressed ? 0 : -EAGAIN;
	}

	if (s->state == GXFP_TLS_HS_DONE)
		return 1;

	return -EAGAIN;
}

int gxfp_tls_service_get_raw_image(struct gxfp_tls_service *svc,
				   uint8_t out_hdr[5],
				   uint8_t **out_body,
				   size_t *out_body_len)
{
	struct gxfp_tls_service_impl *s;
	if (!svc || !svc->impl)
		return -EINVAL;
	s = (struct gxfp_tls_service_impl *)svc->impl;
	if (!out_hdr || !out_body || !out_body_len)
		return -EINVAL;
	if (!s->img.have_oneframe)
		return 0;
	return gxfp_plain_image_sink_extract(&s->img,
							out_hdr,
							out_body,
							out_body_len);
}

enum gxfp_tls_hs_state gxfp_tls_service_state(const struct gxfp_tls_service *svc)
{
	const struct gxfp_tls_service_impl *s;
	if (!svc || !svc->impl)
		return GXFP_TLS_HS_INIT;
	s = (const struct gxfp_tls_service_impl *)svc->impl;
	return s->state;
}

int gxfp_tls_service_error(const struct gxfp_tls_service *svc)
{
	const struct gxfp_tls_service_impl *s;
	if (!svc || !svc->impl)
		return -EINVAL;
	s = (const struct gxfp_tls_service_impl *)svc->impl;
	return s->err;
}

const char *gxfp_tls_service_strerror(int err, char *buf, size_t buf_len)
{
	return gxfp_tls_engine_strerror(err, buf, buf_len);
}
