#include "gxfp/algo/image/plain.h"

#include "gxfp/proto/goodix_proto.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>


static void crc32_mpeg2_init_table(uint32_t table[256])
{
	for (uint32_t i = 0; i < 0x100; i++) {
		uint32_t local_c = i << 24;
		uint32_t local_crc = 0;
		for (uint32_t bit = 0; bit < 8; bit++) {
			if (((local_c ^ local_crc) & 0x80000000u) == 0) {
				local_crc <<= 1;
			} else {
				local_crc = (local_crc << 1) ^ 0x04C11DB7u;
			}
			local_c <<= 1;
		}
		table[i] = local_crc;
	}
}

static uint32_t crc32_mpeg2(const uint8_t *buf, size_t len)
{
	static uint32_t table[256];
	static int table_inited = 0;

	if (!table_inited) {
		crc32_mpeg2_init_table(table);
		table_inited = 1;
	}

	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < len; i++) {
		uint8_t b = buf[i];
		crc = table[((crc >> 24) ^ (uint32_t)b) & 0xFFu] ^ (crc << 8);
	}
	return crc;
}

static int gxfp_image_crc_check(const uint8_t *buf, size_t len)
{
	if (!buf || len < 4)
		return 0;

	uint32_t crc = crc32_mpeg2(buf, len - 4);
	uint32_t expected = ((uint32_t)buf[len - 2] << 24) |
			    ((uint32_t)buf[len - 1] << 16) |
			    ((uint32_t)buf[len - 4] << 8) |
			    ((uint32_t)buf[len - 3]);
	return (crc == expected) ? 1 : 0;
}

int gxfp_plain_image_sink_append(struct gxfp_plain_image_sink *s, const uint8_t *p, size_t n)
{
	if (!s || !p || n == 0)
		return 0;
	if (s->have_oneframe)
		return 0;
	if (n < 5)
		return 0;

	const uint8_t *hdr = p;
	const uint8_t *body = p + 5;
	size_t body_len = n - 5;

	if (!gxfp_image_crc_check(body, body_len))
		return 0;

	uint8_t *buf = (uint8_t *)malloc(body_len);
	if (!buf)
		return -ENOMEM;
	memcpy(buf, body, body_len);
	memcpy(s->hdr, hdr, 5);
	s->body = buf;
	s->body_len = body_len;
	s->have_oneframe = 1;
	return 0;
}

void gxfp_plain_image_sink_reset(struct gxfp_plain_image_sink *s)
{
	if (!s)
		return;
	if (s->body && s->body_len)
		memset(s->body, 0, s->body_len);
	free(s->body);
	memset(s->hdr, 0, sizeof(s->hdr));
	s->body = NULL;
	s->body_len = 0;
	s->have_oneframe = 0;
}

int gxfp_plain_image_sink_extract(struct gxfp_plain_image_sink *s,
					     uint8_t out_hdr[5],
					     uint8_t **out_body, size_t *out_body_len)
{
	if (!s || !out_body || !out_body_len)
		return -EINVAL;
	*out_body = NULL;
	*out_body_len = 0;
	if (out_hdr)
		memset(out_hdr, 0, 5);

	if (!s->have_oneframe || !s->body || s->body_len == 0)
		return 0;

	size_t len = s->body_len;
	uint8_t *copy = (uint8_t *)malloc(len);
	if (!copy)
		return -ENOMEM;
	memcpy(copy, s->body, len);
	if (out_hdr)
		memcpy(out_hdr, s->hdr, 5);

	memset(s->body, 0, len);
	free(s->body);
	s->body = NULL;
	s->body_len = 0;
	s->have_oneframe = 0;

	*out_body = copy;
	*out_body_len = len;
	return 1;
}

void gxfp_plain_scan_for_nonpov_image_chunks(const uint8_t *buf, size_t len, struct gxfp_plain_image_sink *sink)
{
	size_t off;

	if (!buf || len < 8 || !sink)
		return;

	for (off = 0; off + 4 <= len; off++) {
		struct gxfp_frame_parsed frame;
		size_t total;
		uint8_t cmd0;

		if (!gxfp_parse_goodix_body(buf + off, len - off, &frame) || !frame.valid)
			continue;

		total = (size_t)frame.payload_len + 4u;

		cmd0 = (uint8_t)((frame.cmd >> 4) & 0x0F);
		if (cmd0 == 2) {
			if (frame.payload_len >= 5 && frame.payload[0] != 0xAA)
				(void)gxfp_plain_image_sink_append(sink, frame.payload, frame.payload_len);
		}

		if (total > 0 && off + total <= len)
			off += total - 1u;
	}
}
