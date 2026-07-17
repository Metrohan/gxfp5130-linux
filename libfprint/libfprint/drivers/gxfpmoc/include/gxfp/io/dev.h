#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include "gxfp/io/uapi.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gxfp_dev {
	int fd;
};

int gxfp_dev_open(struct gxfp_dev *dev, const char *path, int flags);

void gxfp_dev_close(struct gxfp_dev *dev);

int gxfp_dev_set_nonblock(struct gxfp_dev *dev, int nonblock);

int gxfp_dev_poll_readable(struct gxfp_dev *dev, int timeout_ms);

ssize_t gxfp_dev_read_record(struct gxfp_dev *dev,
				     void *buf,
				     size_t buf_cap,
				     struct gxfp_tap_hdr *out_hdr,
				     const uint8_t **out_payload,
				     size_t *out_payload_len);

int gxfp_dev_send_packet(struct gxfp_dev *dev,
			 uint8_t mp_type,
			 const void *payload,
			 uint16_t payload_len);

int gxfp_dev_flush_rxq(struct gxfp_dev *dev);

#ifdef __cplusplus
}
#endif
