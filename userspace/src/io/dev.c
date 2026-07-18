#include "gxfp/io/dev.h"
#include "gxfp/proto/goodix_constants.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <stdlib.h>

static int parse_tap_record(const void *data,
			   size_t len,
			   struct gxfp_tap_hdr *out_hdr,
			   const uint8_t **out_payload,
			   size_t *out_payload_len)
{
	struct gxfp_tap_hdr hdr;
	size_t need;

	if (!data)
		return -EINVAL;
	if (len < sizeof(struct gxfp_tap_hdr))
		return -EBADMSG;

	memcpy(&hdr, data, sizeof(hdr));
	need = sizeof(struct gxfp_tap_hdr) + (size_t)hdr.len;
	if (need != len)
		return -EBADMSG;

	if (out_hdr)
		*out_hdr = hdr;
	if (out_payload)
		*out_payload = (const uint8_t *)data + sizeof(struct gxfp_tap_hdr);
	if (out_payload_len)
		*out_payload_len = (size_t)hdr.len;
	return 0;
}

int gxfp_dev_open(struct gxfp_dev *dev, const char *path, int flags)
{
	if (!dev || !path)
		return -EINVAL;

	dev->fd = open(path, flags | O_CLOEXEC);
	if (dev->fd < 0)
		return -errno;
	return 0;
}

int gxfp_dev_set_nonblock(struct gxfp_dev *dev, int nonblock)
{
	int flags;

	if (!dev || dev->fd < 0)
		return -EINVAL;

	flags = fcntl(dev->fd, F_GETFL);
	if (flags < 0)
		return -errno;

	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if (fcntl(dev->fd, F_SETFL, flags) != 0)
		return -errno;
	return 0;
}

int gxfp_dev_poll_readable(struct gxfp_dev *dev, int timeout_ms)
{
	struct pollfd pfd;
	int r;

	if (!dev || dev->fd < 0)
		return -EINVAL;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = dev->fd;
	pfd.events = POLLIN;

	r = poll(&pfd, 1, timeout_ms);
	if (r < 0)
		return -errno;
	if (r == 0)
		return -EAGAIN;
	if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
		return -EIO;
	return 0;
}

void gxfp_dev_close(struct gxfp_dev *dev)
{
	if (!dev)
		return;
	if (dev->fd >= 0)
		close(dev->fd);
	dev->fd = -1;
}

ssize_t gxfp_dev_read_record(struct gxfp_dev *dev,
			     void *buf,
			     size_t buf_cap,
			     struct gxfp_tap_hdr *out_hdr,
			     const uint8_t **out_payload,
			     size_t *out_payload_len)
{
	ssize_t r;
	int pr;

	if (!dev || dev->fd < 0 || !buf || buf_cap == 0)
		return -EINVAL;

	r = read(dev->fd, buf, buf_cap);
	if (r < 0)
		return -errno;
	if (r == 0)
		return -EAGAIN;

	pr = parse_tap_record(buf, (size_t)r, out_hdr, out_payload, out_payload_len);
	if (pr < 0)
		return pr;
	return r;
}

int gxfp_dev_flush_rxq(struct gxfp_dev *dev)
{
	if (!dev || dev->fd < 0)
		return -EINVAL;
	if (ioctl(dev->fd, GXFP_IOCTL_FLUSH_RXQ) != 0)
		return -errno;
	return 0;
}

int gxfp_dev_send_packet(struct gxfp_dev *dev,
			 uint8_t mp_type,
			 const void *payload,
			 uint16_t payload_len)
{
	struct gxfp_tx_pkt_hdr txh;
	uint8_t *buf;
	size_t total;
	ssize_t wr;

	if (!dev || dev->fd < 0)
		return -EINVAL;
	if (payload_len && !payload)
		return -EINVAL;
	if (payload_len > GXFP_IOCTL_TX_PAYLOAD_MAX)
		return -EMSGSIZE;

	total = sizeof(txh) + (size_t)payload_len;
	buf = (uint8_t *)malloc(total);
	if (!buf)
		return -ENOMEM;

	memset(&txh, 0, sizeof(txh));
	txh.mp_flags = GOODIX_MP_FLAGS_FROM_TYPE(mp_type);
	txh.payload_len = payload_len;
	memcpy(buf, &txh, sizeof(txh));
	if (payload_len)
		memcpy(buf + sizeof(txh), payload, payload_len);

	wr = write(dev->fd, buf, total);
	free(buf);
	if (wr < 0)
		return -errno;
	if ((size_t)wr != total)
		return -EIO;
	return 0;
}

