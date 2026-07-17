#define _GNU_SOURCE

#include "gxfp/flow/session.h"
#include "gxfp/tls/tls_service.h"
#include "gxfp/algo/common.h"

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int handle_session_error(const struct gxfp_session_events *ev)
{
	if (!ev)
		return -EINVAL;
	if (!ev->session_error)
		return 0;

	fprintf(stderr, "session error: %s (%d)\n", ev->error_msg, ev->error_code);
	return ev->error_code ? ev->error_code : -EIO;
}

static int session_wait_until(struct gxfp_session *s,
			      int timeout_ms,
			      int (*done)(const struct gxfp_session_events *, void *),
			      void *done_ctx)
{
	int64_t start_ms;
	struct gxfp_session_events ev;

	if (!s || !done)
		return -EINVAL;

	start_ms = gxfp_monotonic_ms();

	for (;;) {
		int pr;
		int er;
		int64_t now_ms = gxfp_monotonic_ms();

		if (timeout_ms > 0 && (now_ms - start_ms) > timeout_ms)
			return -ETIMEDOUT;

		gxfp_session_events_clear(&ev);
		gxfp_session_pump(s, 0, &ev);
		er = handle_session_error(&ev);
		if (er < 0)
			return er;
		if (done(&ev, done_ctx))
			return 0;

		pr = gxfp_session_poll_readable(s, 50);
		if (pr == 0 || pr == -EAGAIN) {
			gxfp_session_events_clear(&ev);
			gxfp_session_on_fd(s, GXFP_SESSION_IO_IN, 0, &ev);
			er = handle_session_error(&ev);
			if (er < 0)
				return er;
			if (done(&ev, done_ctx))
				return 0;
		} else {
			return pr;
		}
	}
}

static int done_activate(const struct gxfp_session_events *ev, void *ctx)
{
	(void)ctx;
	return ev && ev->activate_complete;
}

static int done_finger_on(const struct gxfp_session_events *ev, void *ctx)
{
	(void)ctx;
	return ev && ev->finger_status_changed && ev->finger_present;
}

static int done_finger_off(const struct gxfp_session_events *ev, void *ctx)
{
	(void)ctx;
	return ev && ev->finger_status_changed && !ev->finger_present;
}

static int done_image(const struct gxfp_session_events *ev, void *ctx)
{
	(void)ctx;
	return ev && ev->image_ready;
}

static int gxfp_write_pgm_u16(const char *path, int cols, int rows,
		       const uint16_t *pixels, int maxval)
{
	FILE *f;

	if (!path || !pixels || rows <= 0 || cols <= 0)
		return -EINVAL;
	if (maxval <= 0 || maxval > 65535)
		return -EINVAL;

	f = fopen(path, "wb");
	if (!f)
		return -errno;

	if (fprintf(f, "P5\n%d %d\n%d\n", cols, rows, maxval) < 0) {
		fclose(f);
		return -EIO;
	}

	if (maxval <= 255) {
		uint8_t *row = (uint8_t *)malloc((size_t)cols);
		if (!row) {
			fclose(f);
			return -ENOMEM;
		}
		for (int r = 0; r < rows; r++) {
			for (int c = 0; c < cols; c++)
				row[c] = (uint8_t)(pixels[r * cols + c] & 0xFF);
			if (fwrite(row, 1, (size_t)cols, f) != (size_t)cols) {
				free(row);
				fclose(f);
				return -EIO;
			}
		}
		free(row);
	} else {
		uint8_t *row = (uint8_t *)malloc((size_t)cols * 2);
		if (!row) {
			fclose(f);
			return -ENOMEM;
		}
		for (int r = 0; r < rows; r++) {
			for (int c = 0; c < cols; c++) {
				uint16_t v = pixels[r * cols + c];
				row[c * 2]     = (v >> 8) & 0xFF;
				row[c * 2 + 1] = v & 0xFF;
			}
			if (fwrite(row, 1, (size_t)cols * 2, f) != (size_t)cols * 2) {
				free(row);
				fclose(f);
				return -EIO;
			}
		}
		free(row);
	}

	fclose(f);
	return 0;
}

static int save_image_outputs(const struct gxfp_decoded_image *img)
{
	int rc;

	if (!img)
		return -EINVAL;

	rc = gxfp_write_pgm_u16("finger.pgm", img->cols, img->rows, img->pixels, 4095);
	if (rc < 0)
		return rc;

	fprintf(stderr, "capture ok -> finger.pgm (%dx%d)\n", img->cols, img->rows);
	return 0;
}

static void usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s --psk-raw32 <path>\n"
		"\n"
		"Options:\n"
		"  --psk-raw32 <path>   path to 32-byte raw PSK binary file (required)\n"
		"  -h, --help           show this help\n"
		"\n"
		"Output:\n"
		"  finger.pgm           16-bit PGM fingerprint image\n",
		argv0);
}

int main(int argc, char **argv)
{
	const char *psk_raw32_file = NULL;
	uint8_t *psk_raw = NULL;
	size_t psk_raw_len = 0;
	uint32_t timeout_ms = 20000;
	struct gxfp_session sess;
	struct gxfp_session_events ev;
	int rc;

	struct option long_opts[] = {
		{"psk-raw32", required_argument, NULL, 'p'},
		{"help",      no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "p:h", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'p':
			psk_raw32_file = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 2;
		}
	}

	if (!psk_raw32_file) {
		fprintf(stderr, "Missing required option: --psk-raw32\n");
		usage(argv[0]);
		return 2;
	}

	rc = gxfp_read_file_all(psk_raw32_file, &psk_raw, &psk_raw_len);
	if (rc < 0) {
		fprintf(stderr, "read PSK '%s' failed: %s\n", psk_raw32_file, strerror(-rc));
		return 1;
	}

	gxfp_session_init(&sess);
	rc = gxfp_session_open(&sess,
			      "/dev/gxfp",
			      psk_raw,
			      psk_raw_len,
			      1,
			      NULL,
			      0);
	free(psk_raw);
	psk_raw = NULL;
	if (rc < 0) {
		fprintf(stderr, "session_open failed: %s\n", strerror(-rc));
		gxfp_session_dispose(&sess);
		return 1;
	}


	gxfp_session_events_clear(&ev);
	rc = gxfp_session_activate(&sess, 1, &ev);
	if (rc < 0 || handle_session_error(&ev) < 0) {
		gxfp_session_dispose(&sess);
		return 1;
	}

	rc = session_wait_until(&sess, (int)timeout_ms, done_activate, NULL);
	if (rc < 0) {
		char tls_err[128];
		fprintf(stderr, "connect failed: %s\n", gxfp_tls_service_strerror(rc, tls_err, sizeof(tls_err)));
		gxfp_session_dispose(&sess);
		return 1;
	}

	for (;;) {
		fprintf(stderr, "Press Enter to capture.\n");
		if (getchar() != '\n')
			continue;
		struct gxfp_decoded_image img;
		fprintf(stderr, "FDT armed, waiting finger down...\n");
		gxfp_session_events_clear(&ev);
		gxfp_session_change_state(&sess,
					 GXFP_SESSION_STATE_AWAIT_FINGER_ON,
					 &ev);
		rc = handle_session_error(&ev);
		if (rc < 0)
			break;
		rc = session_wait_until(&sess, 0, done_finger_on, NULL);
		if (rc < 0) {
			fprintf(stderr, "fdt gate: %s\n", strerror(-rc));
			break;
		}
		fprintf(stderr, "FDT down detected. capture starts now.\n");

		gxfp_session_events_clear(&ev);
		gxfp_session_change_state(&sess, GXFP_SESSION_STATE_CAPTURE, &ev);
		rc = handle_session_error(&ev);
		if (rc < 0)
			break;

		rc = session_wait_until(&sess, (int)timeout_ms, done_image, NULL);
		if (rc < 0) {
			fprintf(stderr, "capture: %s\n", strerror(-rc));
			break;
		}

		rc = gxfp_session_take_image(&sess, &img);
		if (rc < 0) {
			fprintf(stderr, "take_image: %s\n", strerror(-rc));
			break;
		}

		rc = save_image_outputs(&img);
		gxfp_decoded_image_free(&img);
		if (rc < 0) {
			fprintf(stderr, "save_image: %s\n", strerror(-rc));
			break;
		}

		fprintf(stderr, "waiting finger up after capture...\n");
		gxfp_session_events_clear(&ev);
		gxfp_session_change_state(&sess,
					 GXFP_SESSION_STATE_AWAIT_FINGER_OFF,
					 &ev);
		rc = handle_session_error(&ev);
		if (rc < 0)
			break;
		rc = session_wait_until(&sess, 0, done_finger_off, NULL);
		if (rc < 0 && rc != -ETIMEDOUT) {
			fprintf(stderr, "wait_fdt_up: %s\n", strerror(-rc));
			break;
		}

		fprintf(stderr, "FDT up detected, ready for next capture.\n\n");
	}
}
