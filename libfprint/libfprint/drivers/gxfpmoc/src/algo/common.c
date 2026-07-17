#include "gxfp/algo/common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint16_t gxfp_le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint32_t gxfp_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void gxfp_le32enc(uint8_t out[4], uint32_t v)
{
	out[0] = (uint8_t)(v & 0xff);
	out[1] = (uint8_t)((v >> 8) & 0xff);
	out[2] = (uint8_t)((v >> 16) & 0xff);
	out[3] = (uint8_t)((v >> 24) & 0xff);
}

int64_t gxfp_monotonic_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

int gxfp_read_file_all(const char *path, uint8_t **out_buf, size_t *out_len)
{
	FILE *f;
	uint8_t *buf;
	size_t cap = 4096;
	size_t len = 0;
	uint8_t tmp[1024];
	size_t n;

	if (!path || !out_buf || !out_len)
		return -EINVAL;

	f = fopen(path, "rb");
	if (!f)
		return -errno;

	buf = (uint8_t *)malloc(cap);
	if (!buf) {
		fclose(f);
		return -ENOMEM;
	}

	for (;;) {
		n = fread(tmp, 1, sizeof(tmp), f);
		if (n == 0)
			break;
		if (len + n > cap) {
			size_t new_cap = cap;
			while (new_cap < len + n) {
				if (new_cap > (64u * 1024u * 1024u)) {
					free(buf);
					fclose(f);
					return -EOVERFLOW;
				}
				new_cap *= 2u;
			}
			uint8_t *p = (uint8_t *)realloc(buf, new_cap);
			if (!p) {
				free(buf);
				fclose(f);
				return -ENOMEM;
			}
			buf = p;
			cap = new_cap;
		}
		memcpy(buf + len, tmp, n);
		len += n;
	}

	if (ferror(f)) {
		free(buf);
		fclose(f);
		return -EIO;
	}

	fclose(f);
	*out_buf = buf;
	*out_len = len;
	return 0;
}

int gxfp_align_blob(uint8_t **blob, size_t *len)
{
	uint8_t *nb;
	size_t len_aligned;

	if (!blob || !len)
		return -EINVAL;
	if (!*blob || *len == 0)
		return -EINVAL;

	len_aligned = (*len + 3u) & ~3u;
	if (len_aligned == *len)
		return 0;

	if (len_aligned > 0xFFFFu)
		return -EMSGSIZE;

	nb = (uint8_t *)realloc(*blob, len_aligned);
	if (!nb)
		return -ENOMEM;

	*blob = nb;
	memset(*blob + *len, 0, len_aligned - *len);
	*len = len_aligned;

	return 0;
}
