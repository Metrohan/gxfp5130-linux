#pragma once

#define GOODIX_CHECKSUM_SEED    0xAA

#define GOODIX_MP_TYPE_CMD      0x0A   /* command channel (request/response) */
#define GOODIX_MP_TYPE_TLS      0x0B   /* TLS container channel */
#define GOODIX_MP_TYPE_NOTICE   0x0C   /* asynchronous notice/event channel */
#define GOODIX_MP_FLAGS_FROM_TYPE(t) ((uint8_t)(((t) & 0x0F) << 4))

#define GOODIX_PAYLOAD_MAX      0xFF00u
