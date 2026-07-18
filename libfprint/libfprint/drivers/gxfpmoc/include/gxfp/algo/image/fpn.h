#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gxfp_fpn_dark {
	uint16_t *pixels;
	int       rows;
	int       cols;
};

void gxfp_fpn_free(struct gxfp_fpn_dark *dark);

int gxfp_fpn_correct(uint16_t *pixels, int rows, int cols,
		     const struct gxfp_fpn_dark *dark);

#ifdef __cplusplus
}
#endif
