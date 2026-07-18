#include "tls_engine.h"

#include "gxfp/tls/tls_constants.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>

struct gxfp_tls_engine {
	gxfp_tls_send_record_fn send_fn;
	void *send_ctx;
	uint8_t *rxq;
	size_t rxq_cap;
	size_t rxq_len;
	int hs_done;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
};

static int rxq_push(struct gxfp_tls_engine *e, const uint8_t *data, size_t len)
{
	if (!e || !data)
		return -EINVAL;
	if (e->rxq_len + len > e->rxq_cap)
		return -ENOSPC;
	memcpy(e->rxq + e->rxq_len, data, len);
	e->rxq_len += len;
	return 0;
}

static int rxq_pop(struct gxfp_tls_engine *e, uint8_t *out, size_t out_cap)
{
	size_t n;
	if (!e || !out || out_cap == 0)
		return -EINVAL;
	if (e->rxq_len == 0)
		return 0;
	n = (out_cap < e->rxq_len) ? out_cap : e->rxq_len;
	memcpy(out, e->rxq, n);
	if (n < e->rxq_len)
		memmove(e->rxq, e->rxq + n, e->rxq_len - n);
	e->rxq_len -= n;
	return (int)n;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len)
{
	struct gxfp_tls_engine *e = (struct gxfp_tls_engine *)ctx;
	int n;

	if (!e || !buf || len == 0)
		return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;

	n = rxq_pop(e, buf, len);
	if (n > 0)
		return n;
	if (n < 0)
		return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
	return MBEDTLS_ERR_SSL_WANT_READ;
}

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
	struct gxfp_tls_engine *e = (struct gxfp_tls_engine *)ctx;
	size_t off = 0;

	if (!e || !buf || !e->send_fn)
		return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;

	while (off + TLS_REC_HDR_LEN <= len) {
		uint16_t rec_len = (uint16_t)buf[off + 3] << 8 | (uint16_t)buf[off + 4];
		size_t total = TLS_REC_HDR_LEN + (size_t)rec_len;
		int r;

		if (off + total > len)
			break;
		r = e->send_fn(e->send_ctx, buf + off, total);
		if (r < 0) {
			if (r == -EAGAIN || r == -EINTR)
				return MBEDTLS_ERR_SSL_WANT_WRITE;
			return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
		}
		off += total;
	}

	if (off != len)
		return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
	return (int)len;
}

static int setup_ssl(struct gxfp_tls_engine *e, const uint8_t *psk, size_t psk_len)
{
	const char *pers = "gxfp_tls";
	int r;

	mbedtls_ssl_init(&e->ssl);
	mbedtls_ssl_config_init(&e->conf);
	mbedtls_entropy_init(&e->entropy);
	mbedtls_ctr_drbg_init(&e->ctr_drbg);

	r = mbedtls_ctr_drbg_seed(&e->ctr_drbg, mbedtls_entropy_func, &e->entropy,
				 (const unsigned char *)pers, strlen(pers));
	if (r != 0)
		return -EIO;

	r = mbedtls_ssl_config_defaults(&e->conf,
				MBEDTLS_SSL_IS_SERVER,
				MBEDTLS_SSL_TRANSPORT_STREAM,
				MBEDTLS_SSL_PRESET_DEFAULT);
	if (r != 0)
		return -EIO;

	mbedtls_ssl_conf_rng(&e->conf, mbedtls_ctr_drbg_random, &e->ctr_drbg);
	mbedtls_ssl_conf_min_tls_version(&e->conf, MBEDTLS_SSL_VERSION_TLS1_2);
	mbedtls_ssl_conf_max_tls_version(&e->conf, MBEDTLS_SSL_VERSION_TLS1_2);

	r = mbedtls_ssl_conf_psk(&e->conf, psk, psk_len,
				 (const unsigned char *)GXFP5130_PSK_IDENTITY,
				 strlen(GXFP5130_PSK_IDENTITY));
	if (r != 0)
		return -EIO;

	r = mbedtls_ssl_setup(&e->ssl, &e->conf);
	if (r != 0)
		return -EIO;

	mbedtls_ssl_set_bio(&e->ssl, e, bio_send, bio_recv, NULL);
	return 0;
}

int gxfp_tls_engine_create(struct gxfp_tls_engine **out,
			   const uint8_t *psk,
			   size_t psk_len,
			   gxfp_tls_send_record_fn send_fn,
			   void *send_ctx)
{
	struct gxfp_tls_engine *e;
	int r;

	if (!out || !psk || psk_len != GXFP5130_PSK_LEN || !send_fn)
		return -EINVAL;
	*out = NULL;

	e = (struct gxfp_tls_engine *)calloc(1, sizeof(*e));
	if (!e)
		return -ENOMEM;
	e->send_fn = send_fn;
	e->send_ctx = send_ctx;
	e->rxq_cap = 256u * 1024u;
	e->rxq = (uint8_t *)malloc(e->rxq_cap);
	if (!e->rxq) {
		free(e);
		return -ENOMEM;
	}

	r = setup_ssl(e, psk, psk_len);
	if (r < 0) {
		gxfp_tls_engine_free(e);
		return r;
	}

	*out = e;
	return 0;
}

void gxfp_tls_engine_free(struct gxfp_tls_engine *e)
{
	if (!e)
		return;
	mbedtls_ssl_free(&e->ssl);
	mbedtls_ssl_config_free(&e->conf);
	mbedtls_ctr_drbg_free(&e->ctr_drbg);
	mbedtls_entropy_free(&e->entropy);
	free(e->rxq);
	free(e);
}

int gxfp_tls_engine_feed_cipher_record(struct gxfp_tls_engine *e,
				       const uint8_t *rec,
				       size_t rec_len)
{
	if (!e || !rec || rec_len == 0)
		return -EINVAL;
	return rxq_push(e, rec, rec_len);
}

int gxfp_tls_engine_handshake_step(struct gxfp_tls_engine *e)
{
	int r;

	if (!e)
		return -EINVAL;
	if (e->hs_done)
		return 1;

	r = mbedtls_ssl_handshake(&e->ssl);
	if (r == 0) {
		e->hs_done = 1;
		return 1;
	}
	if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
		return -EAGAIN;
	return r;
}

int gxfp_tls_engine_pull_plain(struct gxfp_tls_engine *e,
			      uint8_t *out,
			      size_t out_cap,
			      size_t *out_len)
{
	int r;

	if (!e || !out || out_cap == 0 || !out_len)
		return -EINVAL;
	*out_len = 0;

	r = mbedtls_ssl_read(&e->ssl, out, out_cap);
	if (r > 0) {
		*out_len = (size_t)r;
		return 1;
	}
	if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
		return -EAGAIN;
	if (r == 0)
		return -EPIPE;
	return r;
}

const char *gxfp_tls_engine_strerror(int err, char *buf, size_t buf_len)
{
	int pos;
	const char *s;

	if (!buf || buf_len == 0)
		return "invalid";

	buf[0] = '\0';
	if (err == 0) {
		snprintf(buf, buf_len, "Success");
		return buf;
	}

	pos = (err < 0) ? -err : err;
	s = strerror(pos);
	if (s && strncmp(s, "Unknown error", 13) != 0) {
		snprintf(buf, buf_len, "%s", s);
		return buf;
	}

	mbedtls_strerror((err < 0) ? err : -pos, buf, buf_len);
	if (buf[0] != '\0' && strncmp(buf, "UNKNOWN ERROR CODE", 18) != 0)
		return buf;

	snprintf(buf, buf_len, "Unknown error %d", pos);
	return buf;
}
