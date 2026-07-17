#pragma once

#include <stdint.h>

#include "gxfp/proto/goodix_cmd.h"

#define TLS_REC_HDR_LEN 5
#define TLS_REC_MAX_PLAIN (16u * 1024u)

#define GXFP5130_PSK_LEN 32u
#define GXFP5130_PSK_IDENTITY "Client_identity"