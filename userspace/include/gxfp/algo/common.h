#pragma once

#include <stddef.h>
#include <stdint.h>

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

int gxfp_read_file_all(const char *path, uint8_t **out_buf, size_t *out_len);

uint16_t gxfp_le16(const uint8_t *p);
uint32_t gxfp_le32(const uint8_t *p);
void gxfp_le32enc(uint8_t out[4], uint32_t v);

int64_t gxfp_monotonic_ms(void);

int gxfp_align_blob(uint8_t **blob, size_t *len);

#ifdef __cplusplus
}
#endif
