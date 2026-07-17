#include "gxfp/algo/image/decoder.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void unpack_sample4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5,
			   uint16_t *s0, uint16_t *s1, uint16_t *s2, uint16_t *s3)
{
	*s0 = ((b0 & 0x0F) << 8) | b1;
	*s1 = (b3 << 4) | (b0 >> 4);
	*s2 = ((b5 & 0x0F) << 8) | b2;
	*s3 = (b4 << 4) | (b5 >> 4);
}

int gxfp_decode_milan(const uint8_t *payload, size_t payload_len,
		      int rows, int cols, struct gxfp_decoded_image *out)
{
	if (!payload || !out)
		return -EINVAL;
	if (rows <= 0 || cols <= 0)
		return -EINVAL;

	int stride = ((rows + 2) * 3) / 2;
	int take_per_block = (rows * 3) / 2 + 1;
	size_t need = (size_t)stride * cols;

	if (payload_len < need)
		return -EINVAL;

	size_t pu_len = (size_t)take_per_block * cols;
	uint8_t *pu = (uint8_t *)malloc(pu_len);
	if (!pu)
		return -ENOMEM;

	for (int c = 0; c < cols; c++) {
		const uint8_t *src = payload + (c * stride);
		memcpy(pu + (c * take_per_block), src, take_per_block);
	}

	/* Unpack 12-bit into col-major uint16 array. */
	int total = rows * cols;
	uint16_t *col_major = (uint16_t *)calloc(total, sizeof(uint16_t));
	if (!col_major) {
		free(pu);
		return -ENOMEM;
	}

	int out_i = 0;
	size_t in_i = 0;

	while (in_i < pu_len && out_i < total) {
		if (out_i % rows == rows - 2) {
			if (in_i + 4 > pu_len)
				break;
			uint8_t b0 = pu[in_i];
			uint8_t b1 = pu[in_i + 1];
			uint8_t b3 = pu[in_i + 3];
			col_major[out_i] = ((b0 & 0x0F) << 8) | b1;
			col_major[out_i + 1] = (b3 << 4) | (b0 >> 4);
			out_i += 2;
			in_i += 4;
		} else {
			if (in_i + 6 > pu_len)
				break;
			uint8_t b0 = pu[in_i];
			uint8_t b1 = pu[in_i + 1];
			uint8_t b2 = pu[in_i + 2];
			uint8_t b3 = pu[in_i + 3];
			uint8_t b4 = pu[in_i + 4];
			uint8_t b5 = pu[in_i + 5];
			uint16_t s0, s1, s2, s3;
			unpack_sample4(b0, b1, b2, b3, b4, b5, &s0, &s1, &s2, &s3);
			col_major[out_i] = s0;
			col_major[out_i + 1] = s1;
			col_major[out_i + 2] = s2;
			col_major[out_i + 3] = s3;
			out_i += 4;
			in_i += 6;
		}
	}

	free(pu);

	if (out_i != total) {
		free(col_major);
		return -EIO;
	}

	/* Transpose col-major -> row-major. */
	uint16_t *row_major = (uint16_t *)malloc(total * sizeof(uint16_t));
	if (!row_major) {
		free(col_major);
		return -ENOMEM;
	}

	int idx = 0;
	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			row_major[idx++] = col_major[c * rows + r];
		}
	}

	free(col_major);

	out->pixels = row_major;
	out->rows = rows;
	out->cols = cols;
	return 0;
}

int gxfp_decode_chicago(const uint8_t *payload, size_t payload_len,
			int rows, int cols, int is_u16le,
			struct gxfp_decoded_image *out)
{
	if (!payload || !out)
		return -EINVAL;
	if (rows <= 0 || cols <= 0)
		return -EINVAL;

	int total = rows * cols;

	if (is_u16le) {
		/* Already in uint16 LE format. */
		size_t need = (size_t)total * 2;
		if (payload_len < need)
			return -EINVAL;

		uint16_t *pixels = (uint16_t *)malloc(total * sizeof(uint16_t));
		if (!pixels)
			return -ENOMEM;

		for (int i = 0; i < total; i++) {
			pixels[i] = ((uint16_t)payload[i * 2 + 1] << 8) | payload[i * 2];
		}

		out->pixels = pixels;
		out->rows = rows;
		out->cols = cols;
		return 0;
	}

	/* 12-bit packed format. */
	if (payload_len < 6)
		return -EINVAL;

	uint16_t *out_pixels = (uint16_t *)calloc(total, sizeof(uint16_t));
	if (!out_pixels)
		return -ENOMEM;

	int out_idx = 0;
	int in_off = 0;
	int transpose = (payload_len != 0x960);

	for (; in_off + 6 <= (int)payload_len; in_off += 6) {
		uint16_t s0, s1, s2, s3;
		unpack_sample4(payload[in_off], payload[in_off + 1],
			       payload[in_off + 2], payload[in_off + 3],
			       payload[in_off + 4], payload[in_off + 5],
			       &s0, &s1, &s2, &s3);
		uint16_t samples[] = { s0, s1, s2, s3 };

		for (int off = 0; off < 4; off++) {
			int i = out_idx + off;
			if (transpose) {
				int dst = (i % rows) * cols + (i / rows);
				if (dst < total)
					out_pixels[dst] = samples[off];
			} else {
				if (i < total)
					out_pixels[i] = samples[off];
			}
		}
		out_idx += 4;
	}

	if (out_idx != total) {
		free(out_pixels);
		return -EIO;
	}

	out->pixels = out_pixels;
	out->rows = rows;
	out->cols = cols;
	return 0;
}

int gxfp_decode_auto_detect(const uint8_t *buf, size_t len,
			     struct gxfp_decode_auto_result *res)
{
	if (!buf || !res)
		return -EINVAL;

	const struct {
		enum gxfp_decode_mode mode;
		size_t packed_len;
		int rows;
		int cols;
	} modes[] = {
		{ GXFP_DECODE_MILANG_RAW, 0x39C0, 0x36, 0xB0 },
		{ GXFP_DECODE_MILANG_NAV, 0x0540, 0x36, 0x10 },
		{ GXFP_DECODE_CHICAGOH_RAW, 0x1E00, 0x40, 0x50 },
		{ GXFP_DECODE_CHICAGOH_NAV, 0x0960, 0x40, 0x19 },
		{ GXFP_DECODE_CHICAGOH_RAW_U16LE, 0x2800, 0x40, 0x50 },
		{ GXFP_DECODE_CHICAGOH_NAV_U16LE, 0x0C80, 0x40, 0x19 },
	};

	for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
		size_t pl = modes[i].packed_len;
		if (len == pl) {
			res->mode = modes[i].mode;
			res->payload = buf;
			res->payload_len = len;
			return 0;
		}
		if (len == pl + 4) {
			/* Strip CRC32 tail. */
			res->mode = modes[i].mode;
			res->payload = buf;
			res->payload_len = pl;
			return 0;
		}
		if (len == 5 + pl) {
			/* Strip 5-byte header. */
			res->mode = modes[i].mode;
			res->payload = buf + 5;
			res->payload_len = pl;
			return 0;
		}
		if (len == 5 + pl + 4) {
			/* Strip 5-byte header + CRC32 tail. */
			res->mode = modes[i].mode;
			res->payload = buf + 5;
			res->payload_len = pl;
			return 0;
		}
	}

	return -ENOENT;
}

int gxfp_decode_image(const uint8_t *buf, size_t len, enum gxfp_decode_mode mode,
		      struct gxfp_decoded_image *out)
{
	if (!buf || !out)
		return -EINVAL;

	switch (mode) {
	case GXFP_DECODE_AUTO: {
		struct gxfp_decode_auto_result res;
		int r = gxfp_decode_auto_detect(buf, len, &res);
		if (r < 0)
			return r;
		return gxfp_decode_image(res.payload, res.payload_len, res.mode, out);
	}
	case GXFP_DECODE_MILANG_RAW:
		return gxfp_decode_milan(buf, len, 0x36, 0xB0, out);
	case GXFP_DECODE_MILANG_NAV:
		return gxfp_decode_milan(buf, len, 0x36, 0x10, out);
	case GXFP_DECODE_CHICAGOH_RAW:
		return gxfp_decode_chicago(buf, len, 0x40, 0x50, 0, out);
	case GXFP_DECODE_CHICAGOH_NAV:
		return gxfp_decode_chicago(buf, len, 0x40, 0x19, 0, out);
	case GXFP_DECODE_CHICAGOH_RAW_U16LE:
		return gxfp_decode_chicago(buf, len, 0x40, 0x50, 1, out);
	case GXFP_DECODE_CHICAGOH_NAV_U16LE:
		return gxfp_decode_chicago(buf, len, 0x40, 0x19, 1, out);
	default:
		return -EINVAL;
	}
}

void gxfp_decoded_image_free(struct gxfp_decoded_image *img)
{
	if (!img)
		return;
	free(img->pixels);
	img->pixels = NULL;
	img->rows = 0;
	img->cols = 0;
}