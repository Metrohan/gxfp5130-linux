#include "gxfp/proto/goodix_proto.h"

#include "gxfp/proto/goodix_constants.h"

#include <errno.h>
#include <string.h>

uint8_t gxfp_goodix_proto_checksum(uint8_t cmd, uint16_t decl_len, const uint8_t *payload, size_t payload_len)
{
	uint32_t sum = (uint32_t)cmd + (uint32_t)(decl_len & 0xff) + (uint32_t)((decl_len >> 8) & 0xff);
	for (size_t i = 0; i < payload_len; i++)
		sum += (uint32_t)payload[i];
	return (uint8_t)(GOODIX_CHECKSUM_SEED - (uint8_t)sum);
}

int gxfp_goodix_build_frame(uint8_t cmd,
			   const uint8_t *payload,
			   size_t payload_len,
			   uint8_t *out,
			   size_t out_cap)
{
	uint16_t decl_len;
	size_t total;

	if (!out)
		return -EINVAL;
	if (payload_len && !payload)
		return -EINVAL;
	if (payload_len > GOODIX_PAYLOAD_MAX)
		return -EOVERFLOW;

	decl_len = (uint16_t)(1u + payload_len);
	total = 1u + 2u + payload_len + 1u;
	if (total > out_cap)
		return -ENOSPC;

	out[0] = cmd;
	out[1] = (uint8_t)(decl_len & 0xff);
	out[2] = (uint8_t)((decl_len >> 8) & 0xff);
	if (payload_len)
		memcpy(out + 3, payload, payload_len);
	out[3 + payload_len] = gxfp_goodix_proto_checksum(cmd, decl_len, payload, payload_len);

	return (int)total;
}

bool gxfp_parse_goodix_body(const uint8_t *buf,
			    size_t buf_len,
			    struct gxfp_frame_parsed *out)
{
	uint8_t cmd;
	uint16_t decl_len;
	uint16_t payload_len;
	size_t need;
	uint8_t proto_ck;

	if (!buf || buf_len < 4 || !out)
		return false;
	memset(out, 0, sizeof(*out));

	cmd = buf[0];
	decl_len = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
	if (decl_len < 1)
		return false;
	payload_len = (uint16_t)(decl_len - 1u);

	need = 3u + (size_t)payload_len + 1u;
	if (need > buf_len)
		return false;

	out->cmd = cmd;
	out->decl_len = decl_len;
	out->payload = buf + 3;
	out->payload_len = payload_len;
	proto_ck = buf[3u + payload_len];
	out->proto_checksum = proto_ck;
	out->proto_checksum_ok =
		(gxfp_goodix_proto_checksum(cmd, decl_len, out->payload, payload_len) == proto_ck);
	out->valid = true;
	return true;
}
