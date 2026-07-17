#include "gxfp/flow/device_recovery.h"
#include "gxfp/io/dev.h"

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

static void usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s [options]\n"
		"\n"
		"Options:\n"
		"  --no-unstick    skip TLS un-sticking\n"
		"  --reset-mcu     reset MCU after recovery\n"
		"  --reset-sensor  reset sensor after recovery\n"
		"  -h, --help      show this help\n",
		argv0);
}

int main(int argc, char **argv)
{
	struct gxfp_dev dev;
	int unstick_tls = 1;
	int reset_mcu = 0;
	int reset_sensor = 0;
	int r;

	struct option long_opts[] = {
		{"no-unstick",   no_argument, NULL, 'U'},
		{"reset-mcu",    no_argument, NULL, 'M'},
		{"reset-sensor", no_argument, NULL, 'S'},
		{"help",         no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "UMS", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'U': unstick_tls = 0; break;
		case 'M': reset_mcu = 1; break;
		case 'S': reset_sensor = 1; break;
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 2;
		}
	}

	memset(&dev, 0, sizeof(dev));
	r = gxfp_dev_open(&dev, "/dev/gxfp", O_RDWR | O_NONBLOCK);
	if (r < 0) {
		fprintf(stderr, "open(/dev/gxfp) failed: %s\n", strerror(-r));
		return 1;
	}

	r = gxfp_device_recovery(&dev, unstick_tls, reset_mcu, reset_sensor);
	if (r < 0) {
		fprintf(stderr, "recovery failed: %s\n", strerror(-r));
		gxfp_dev_close(&dev);
		return 1;
	}

	fprintf(stderr,
		"recovery complete (unstick=%d, reset_mcu=%d, reset_sensor=%d)\n",
		unstick_tls,
		reset_mcu,
		reset_sensor);
	gxfp_dev_close(&dev);
	return 0;
}
