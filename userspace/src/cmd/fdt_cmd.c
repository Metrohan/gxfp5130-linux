#include "gxfp/cmd/fdt_cmd.h"

#include "gxfp/cmd/goodix_xfer.h"
#include "gxfp/proto/goodix_cmd.h"

#include <errno.h>
#include <string.h>

#define GXFP_FDT_BASE_TABLE_LEN 24
#define FDT_CMD_DOWN_MAGIC_0 0x08
#define FDT_CMD_DOWN_MAGIC_1 0x01
#define FDT_CMD_MODE_MAGIC_0 0x09
#define FDT_CMD_MODE_MAGIC_1 0x01
#define FDT_CMD_UP_MAGIC_0 0x0A
#define FDT_CMD_UP_MAGIC_1 0x01
#define GXFP_FDT_WIRE_TRAILER_LEN 8

#define FDT_PAYLOAD_SIZE_DOWN (2 + GXFP_FDT_BASE_TABLE_LEN + GXFP_FDT_WIRE_TRAILER_LEN)
#define FDT_PAYLOAD_SIZE_MODE (2 + GXFP_FDT_BASE_TABLE_LEN + GXFP_FDT_WIRE_TRAILER_LEN)
#define FDT_PAYLOAD_SIZE_UP (2 + GXFP_FDT_BASE_TABLE_LEN + GXFP_FDT_WIRE_TRAILER_LEN)

void gxfp_cmd_fdt_state_init(struct gxfp_cmd_fdt_state *state)
{
    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->base_table_inited = 1;
}

void gxfp_cmd_fdt_state_set_runtime(struct gxfp_cmd_fdt_state *state,
                                    int second_flag,
                                    int need_fdt_up_twice,
                                    int diff_use,
                                    int16_t dac_offset)
{
    if (!state)
        return;

    state->up_second_flag = second_flag ? 1 : 0;
    state->up_need_twice = need_fdt_up_twice ? 1 : 0;
    state->up_diff_use = diff_use ? 1 : 0;
    state->up_dac_offset = dac_offset;
    state->up_round = 0;
}

static void fdt_down_payload_build(const struct gxfp_cmd_fdt_state *state,
                                   uint8_t payload[FDT_PAYLOAD_SIZE_DOWN])
{
    if (!state)
        return;

    memset(payload, 0, FDT_PAYLOAD_SIZE_DOWN);
    memcpy(payload + 0, (uint8_t[]){ FDT_CMD_DOWN_MAGIC_0, FDT_CMD_DOWN_MAGIC_1 }, 2);
    memcpy(payload + 2, state->down_table_5130, GXFP_FDT_BASE_TABLE_LEN);
}

static void fdt_mode_payload_build(const struct gxfp_cmd_fdt_state *state,
                                   uint8_t payload[FDT_PAYLOAD_SIZE_MODE])
{
    if (!state)
        return;

    memset(payload, 0, FDT_PAYLOAD_SIZE_MODE);
    memcpy(payload + 0, (uint8_t[]){ FDT_CMD_MODE_MAGIC_0, FDT_CMD_MODE_MAGIC_1 }, 2);
    memcpy(payload + 2, state->manual_table_5130, GXFP_FDT_BASE_TABLE_LEN);
}

static void fdt_up_payload_build(const struct gxfp_cmd_fdt_state *state,
                                 uint8_t payload[FDT_PAYLOAD_SIZE_UP])
{
    if (!state)
        return;

    memset(payload, 0, FDT_PAYLOAD_SIZE_UP);
    memcpy(payload + 0, (uint8_t[]){ FDT_CMD_UP_MAGIC_0, FDT_CMD_UP_MAGIC_1 }, 2);
    memcpy(payload + 2, state->up_table_5130, GXFP_FDT_BASE_TABLE_LEN);
}

int gxfp_cmd_fdt_set_mode(struct gxfp_dev *dev,
                          struct gxfp_cmd_fdt_state *state)
{
    uint8_t payload_mode[FDT_PAYLOAD_SIZE_MODE];

    if (!dev || !state)
        return -EINVAL;

    memcpy(state->manual_table_5130, state->down_table_5130, GXFP_FDT_BASE_TABLE_LEN);

    fdt_mode_payload_build(state, payload_mode);
    return gxfp_goodix_send_async(dev,
                                  GXFP_CMD_FDT_MODE,
                                  payload_mode,
                                  (uint16_t)FDT_PAYLOAD_SIZE_MODE);
}

int gxfp_cmd_fdt_send_down(struct gxfp_dev *dev,
                           const struct gxfp_cmd_fdt_state *state)
{
    uint8_t payload_down[FDT_PAYLOAD_SIZE_DOWN];

    if (!dev || !state)
        return -EINVAL;

    fdt_down_payload_build(state, payload_down);
    return gxfp_goodix_send_async(dev,
                                  GXFP_CMD_FDT_DOWN,
                                  payload_down,
                                  (uint16_t)FDT_PAYLOAD_SIZE_DOWN);
}

int gxfp_cmd_fdt_send_up(struct gxfp_dev *dev,
                         const struct gxfp_cmd_fdt_state *state)
{
    uint8_t payload_up[FDT_PAYLOAD_SIZE_UP];

    if (!dev || !state)
        return -EINVAL;

    fdt_up_payload_build(state, payload_up);
    return gxfp_goodix_send_async(dev,
                                  GXFP_CMD_FDT_UP,
                                  payload_up,
                                  (uint16_t)FDT_PAYLOAD_SIZE_UP);
}
