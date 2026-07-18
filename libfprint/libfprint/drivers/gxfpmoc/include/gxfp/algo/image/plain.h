#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gxfp_plain_image_sink {
	uint8_t hdr[5];
	uint8_t *body;
	size_t body_len;
	int have_oneframe;
};

void gxfp_plain_image_sink_reset(struct gxfp_plain_image_sink *s);

int gxfp_plain_image_sink_append(struct gxfp_plain_image_sink *s, const uint8_t *p, size_t n);

int gxfp_plain_image_sink_extract(struct gxfp_plain_image_sink *s,
						 uint8_t out_hdr[5],
						 uint8_t **out_body,
						 size_t *out_body_len);

void gxfp_plain_scan_for_nonpov_image_chunks(const uint8_t *buf,
					     size_t len,
					     struct gxfp_plain_image_sink *sink);

#ifdef __cplusplus
}
#endif
