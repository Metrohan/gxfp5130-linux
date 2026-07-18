#include "gxfp/algo/image/fpn.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gxfp_fpn_free(struct gxfp_fpn_dark *dark)
{
	if (!dark)
		return;
	free(dark->pixels);
	dark->pixels = NULL;
	dark->rows = 0;
	dark->cols = 0;
}

int gxfp_fpn_correct(uint16_t *pixels, int rows, int cols,
		     const struct gxfp_fpn_dark *dark)
{
	int total;
	uint16_t gmin, gmax;
	double scale;

	if (!pixels || rows <= 0 || cols <= 0)
		return -EINVAL;

	total = rows * cols;

	/* Dark-frame subtraction */
	if (dark) {
		if (dark->rows != rows || dark->cols != cols)
			return -EINVAL;
		if (!dark->pixels)
			return -EINVAL;

		for (int i = 0; i < total; i++) {
			int32_t v = (int32_t)pixels[i] - (int32_t)dark->pixels[i];

			pixels[i] = (uint16_t)(v >= 0 ? v : -v);
		}
	}

	/* Global linear stretch to 0–4095 */
	gmin = 4095;
	gmax = 0;
	for (int y = 1; y < rows - 1; y++) {
		for (int x = 1; x < cols - 1; x++) {
			uint16_t v = pixels[y * cols + x];

			if (v < gmin) gmin = v;
			if (v > gmax) gmax = v;
		}
	}

	if (gmax > gmin) {
		scale = 4095.0 / (double)(gmax - gmin);
		for (int y = 0; y < rows; y++) {
			for (int x = 0; x < cols; x++) {
				double v = ((double)pixels[y * cols + x] - gmin) * scale;

				if (v < 0.0)    v = 0.0;
				if (v > 4095.0) v = 4095.0;
				pixels[y * cols + x] = (uint16_t)(v + 0.5);
			}
		}
	}
	return 0;
}
