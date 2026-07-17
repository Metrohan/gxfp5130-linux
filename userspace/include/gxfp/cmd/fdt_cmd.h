#pragma once

#include "gxfp/io/dev.h"
#include "gxfp/io/uapi.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gxfp_cmd_fdt_state {
    uint8_t down_table_5130[24];
    uint8_t up_table_5130[24];
    uint8_t manual_table_5130[24];
    int base_table_inited;
    int up_second_flag;
    int up_need_twice;
    int up_diff_use;
    int16_t up_dac_offset;
    int up_round;
};

void gxfp_cmd_fdt_state_init(struct gxfp_cmd_fdt_state *state);
void gxfp_cmd_fdt_state_set_runtime(struct gxfp_cmd_fdt_state *state,
                                    int second_flag,
                                    int need_fdt_up_twice,
                                    int diff_use,
                                    int16_t dac_offset);

int gxfp_cmd_fdt_set_mode(struct gxfp_dev *dev,
                          struct gxfp_cmd_fdt_state *state);
int gxfp_cmd_fdt_send_down(struct gxfp_dev *dev,
                           const struct gxfp_cmd_fdt_state *state);
int gxfp_cmd_fdt_send_up(struct gxfp_dev *dev,
                         const struct gxfp_cmd_fdt_state *state);

#ifdef __cplusplus
}
#endif
