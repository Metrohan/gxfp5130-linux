#define _GNU_SOURCE

#include "gxfp/io/dev.h"
#include "gxfp/cmd/production_cmd.h"
#include "gxfp/algo/common.h"
#include "gxfp/algo/payload/provision.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s <action> [options]\n"
		"\n"
		"Actions (exactly one required):\n"
		"  --build-bb010002 <out.bin>        build full BB010002 provisioning blob\n"
		"  --dump-bb010002 <out.bin>         dump BB010002 payload from device\n"
		"  --dump-bb010003 <out.bin>         dump BB010003 payload from device\n"
		"  --upload-bb010002 <in.bin>        upload BB010002 blob to device\n"
		"\n"
		"Build options:\n"
		"  --psk-raw32 <path>      32-byte raw PSK file (default: generate random)\n"
		"  --seed8 <path>          8-byte seed file (default: generate random)\n"
		"  --sealed-psk <path>     pre-sealed PSK blob file\n"
		"  --out-psk-raw32 <path>  write generated PSK to file\n"
		"  --no-pad4               skip 4-byte alignment padding\n"
		"\n"
		"Other:\n"
		"  -h, --help              show this help\n",
		argv0);
}

static int fill_random_bytes(uint8_t *buf, size_t len)
{
	int fd;
	ssize_t n;
	size_t off = 0;

	if (!buf || len == 0)
		return -EINVAL;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -errno;

	while (off < len) {
		n = read(fd, buf + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			close(fd);
			return -errno;
		}
		if (n == 0) {
			close(fd);
			return -EIO;
		}
		off += (size_t)n;
	}

	close(fd);
	return 0;
}

static int load_or_generate_bytes(const char *path,
				  size_t expected_len,
				  const char *name,
				  uint8_t **out,
				  size_t *out_len)
{
	int rc;

	if (!out || !out_len || !name || expected_len == 0)
		return -EINVAL;

	if (path) {
		rc = gxfp_read_file_all(path, out, out_len);
		if (rc < 0)
			return rc;
		if (*out_len != expected_len) {
			free(*out);
			*out = NULL;
			*out_len = 0;
			return -EINVAL;
		}
		return 0;
	}

	*out = (uint8_t *)malloc(expected_len);
	if (!*out)
		return -ENOMEM;
	*out_len = expected_len;

	rc = fill_random_bytes(*out, expected_len);
	if (rc < 0) {
		free(*out);
		*out = NULL;
		*out_len = 0;
		return rc;
	}

	fprintf(stderr, "generated random %s (%zu bytes)\n", name, expected_len);
	return 0;
}

int main(int argc, char **argv)
{
	const char *dump_bb010002 = NULL;
	const char *dump_bb010003 = NULL;
	const char *upload_bb010002 = NULL;
	const char *build_bb010002 = NULL;
	const char *psk_raw32 = NULL;
	const char *out_psk_raw32 = NULL;
	const char *seed8_file = NULL;
	const char *sealed_psk_file = NULL;
	int no_pad4 = 0;
	struct gxfp_dev dev;
	int rc;

	struct option long_opts[] = {
		{"build-bb010002",     required_argument, NULL, 'B'},
		{"dump-bb010002",      required_argument, NULL, 'D'},
		{"dump-bb010003",      required_argument, NULL, 'E'},
		{"upload-bb010002",    required_argument, NULL, 'U'},
		{"psk-raw32",          required_argument, NULL, 'p'},
		{"seed8",              required_argument, NULL, 's'},
		{"sealed-psk",         required_argument, NULL, 'S'},
		{"out-psk-raw32",      required_argument, NULL, 'o'},
		{"no-pad4",            no_argument,       NULL, 'N'},
		{"help",               no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "B:D:E:U:p:s:S:o:Nh", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'B': build_bb010002 = optarg; break;
		case 'D': dump_bb010002 = optarg; break;
		case 'E': dump_bb010003 = optarg; break;
		case 'U': upload_bb010002 = optarg; break;
		case 'p': psk_raw32 = optarg; break;
		case 's': seed8_file = optarg; break;
		case 'S': sealed_psk_file = optarg; break;
		case 'o': out_psk_raw32 = optarg; break;
		case 'N': no_pad4 = 1; break;
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 2;
		}
	}

	int action_cnt = 0;
	action_cnt += build_bb010002 != NULL;
	action_cnt += dump_bb010002 != NULL;
	action_cnt += dump_bb010003 != NULL;
	action_cnt += upload_bb010002 != NULL;
	if (action_cnt != 1) {
		fprintf(stderr, "need exactly one action\n");
		return 2;
	}

	if (build_bb010002) {
		uint8_t *psk = NULL;
		size_t psk_len = 0;
		FILE *psk_out = NULL;
		uint8_t *seed8 = NULL;
		size_t seed8_len = 0;
		uint8_t *sealed = NULL;
		size_t sealed_len = 0;
		uint8_t *wb = NULL;
		size_t wb_len = 0;
		uint8_t *inner = NULL;
		size_t inner_len = 0;
		uint8_t *bb = NULL;
		size_t bb_len = 0;
		FILE *f;

		rc = load_or_generate_bytes(psk_raw32, 32, "psk_raw32", &psk, &psk_len);
		if (rc < 0) {
			fprintf(stderr, "load/generate psk_raw32 failed: %s\n", strerror(-rc));
			free(psk);
			return 1;
		}
		rc = load_or_generate_bytes(seed8_file, 8, "seed8", &seed8, &seed8_len);
		if (rc < 0) {
			fprintf(stderr, "load/generate seed8 failed: %s\n", strerror(-rc));
			free(psk);
			free(seed8);
			return 1;
		}

		rc = gxfp_payload_build_bb010003(psk,
						 gxfp_payload_seed32_runtime,
						 &wb,
						 &wb_len,
						 &inner,
						 &inner_len);
		free(inner);
		if (rc < 0) {
			fprintf(stderr, "build bb010003 failed: %s\n", strerror(-rc));
			free(psk);
			free(seed8);
			free(wb);
			return 1;
		}

		if (sealed_psk_file) {
			rc = gxfp_read_file_all(sealed_psk_file, &sealed, &sealed_len);
			if (rc < 0) {
				fprintf(stderr, "read sealed-psk failed: %s\n", strerror(-rc));
				free(psk);
				free(seed8);
				free(wb);
				return 1;
			}
			rc = gxfp_payload_build_bb010002(sealed,
							 sealed_len,
							 seed8,
							 wb,
							 wb_len,
							 no_pad4 ? 0 : 1,
							 &bb,
							 &bb_len);
		} else {
			rc = gxfp_payload_build_bb010002(NULL,
							 0,
							 seed8,
							 wb,
							 wb_len,
							 no_pad4 ? 0 : 1,
							 &bb,
							 &bb_len);
		}

		if (rc < 0) {
			fprintf(stderr, "build bb010002 failed: %s\n", strerror(-rc));
			free(sealed);
			free(psk);
			free(seed8);
			free(wb);
			free(bb);
			return 1;
		}

		f = fopen(build_bb010002, "wb");
		if (!f || fwrite(bb, 1, bb_len, f) != bb_len) {
			if (f)
				fclose(f);
			fprintf(stderr, "write bb010002 output failed\n");
			free(sealed);
			free(psk);
			free(seed8);
			free(wb);
			free(bb);
			return 1;
		}
		fclose(f);

		fprintf(stderr, "built bb010002 -> %s (len=%zu)\n",
			build_bb010002,
			bb_len);

		if (out_psk_raw32) {
			psk_out = fopen(out_psk_raw32, "wb");
			if (!psk_out || fwrite(psk, 1, psk_len, psk_out) != psk_len) {
				if (psk_out)
					fclose(psk_out);
				fprintf(stderr, "write psk raw output failed\n");
				free(sealed);
				free(psk);
				free(seed8);
				free(wb);
				free(bb);
				return 1;
			}
			fclose(psk_out);
		}

		free(sealed);
		free(psk);
		free(seed8);
		free(wb);
		free(bb);
		return 0;
	}

	rc = gxfp_dev_open(&dev, "/dev/gxfp", O_RDWR);
	if (rc < 0) {
		fprintf(stderr, "open(/dev/gxfp): %s\n", strerror(-rc));
		return 1;
	}

	(void)gxfp_dev_flush_rxq(&dev);

	if (upload_bb010002) {
		uint8_t *blob = NULL;
		size_t blob_len = 0;
		uint32_t mcu_ret = 0xffffffffu;

		rc = gxfp_read_file_all(upload_bb010002, &blob, &blob_len);
		if (rc < 0) {
			fprintf(stderr, "read file(%s): %s\n", upload_bb010002, strerror(-rc));
			gxfp_dev_close(&dev);
			return 1;
		}

		rc = gxfp_align_blob(&blob, &blob_len);
		if (rc < 0) {
			fprintf(stderr, "align blob: %s\n", strerror(-rc));
			free(blob);
			gxfp_dev_close(&dev);
			return 1;
		}

		rc = gxfp_cmd_production_write_mcu(&dev, blob, (uint16_t)blob_len, &mcu_ret);
		if (rc < 0) {
			fprintf(stderr, "upload bb010002: %s\n", strerror(-rc));
			free(blob);
			gxfp_dev_close(&dev);
			return 1;
		}
		if (mcu_ret != 0) {
			fprintf(stderr, "upload bb010002: mcu ret 0x%x\n", mcu_ret);
			free(blob);
			gxfp_dev_close(&dev);
			return 1;
		}
		fprintf(stderr, "uploaded bb010002 from %s (size=%zu aligned)\n", upload_bb010002, blob_len);
		free(blob);
		gxfp_dev_close(&dev);
		return 0;
	}

	uint32_t dt = dump_bb010002 ? GXFP_DT_BB010002 : GXFP_DT_BB010003;
	const char *outp = dump_bb010002 ? dump_bb010002 : dump_bb010003;
	{
		uint8_t *buf = (uint8_t *)malloc(0x800u);
		uint32_t data_len = 0;
		FILE *f = NULL;

		if (!buf) {
			fprintf(stderr, "dump 0x%08x: %s\n", dt, strerror(ENOMEM));
			gxfp_dev_close(&dev);
			return 1;
		}

		rc = gxfp_cmd_preset_psk_read(&dev, dt, buf, 0x800u, &data_len);
		if (rc < 0) {
			fprintf(stderr, "dump 0x%08x: %s\n", dt, strerror(-rc));
			free(buf);
			gxfp_dev_close(&dev);
			return 1;
		}

		f = fopen(outp, "wb");
		if (!f) {
			fprintf(stderr, "dump 0x%08x: %s\n", dt, strerror(errno));
			free(buf);
			gxfp_dev_close(&dev);
			return 1;
		}

		if (fwrite(buf, 1, (size_t)data_len, f) != (size_t)data_len) {
			fprintf(stderr, "dump 0x%08x: %s\n", dt, strerror(EIO));
			fclose(f);
			free(buf);
			gxfp_dev_close(&dev);
			return 1;
		}

		fclose(f);
		free(buf);
	}
	fprintf(stderr, "dumped 0x%08x payload to %s\n", dt, outp);

	gxfp_dev_close(&dev);
	return 0;
}
