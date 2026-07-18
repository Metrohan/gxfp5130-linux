#pragma once

#include <stddef.h>
#include <stdint.h>

struct gxfp_tls_engine;

typedef int (*gxfp_tls_send_record_fn)(void *ctx, const uint8_t *rec, size_t rec_len);

int gxfp_tls_engine_create(struct gxfp_tls_engine **out,
			   const uint8_t *psk,
			   size_t psk_len,
			   gxfp_tls_send_record_fn send_fn,
			   void *send_ctx);

void gxfp_tls_engine_free(struct gxfp_tls_engine *e);

int gxfp_tls_engine_feed_cipher_record(struct gxfp_tls_engine *e,
				       const uint8_t *rec,
				       size_t rec_len);

int gxfp_tls_engine_handshake_step(struct gxfp_tls_engine *e);

int gxfp_tls_engine_pull_plain(struct gxfp_tls_engine *e,
			      uint8_t *out,
			      size_t out_cap,
			      size_t *out_len);

const char *gxfp_tls_engine_strerror(int err, char *buf, size_t buf_len);
