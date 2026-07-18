#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gxfp_frame_parsed {
	bool valid;
	bool proto_checksum_ok;
	uint8_t cmd;
	uint16_t decl_len;
	const uint8_t *payload;
	uint16_t payload_len;
	uint8_t proto_checksum;
};

bool gxfp_parse_goodix_body(const uint8_t *buf,
			    size_t buf_len,
			    struct gxfp_frame_parsed *out);

uint8_t gxfp_goodix_proto_checksum(uint8_t cmd,
				   uint16_t decl_len,
				   const uint8_t *payload,
				   size_t payload_len);

int gxfp_goodix_build_frame(uint8_t cmd,
			   const uint8_t *payload,
			   size_t payload_len,
			   uint8_t *out,
			   size_t out_cap);

#ifdef __cplusplus
}
#endif
