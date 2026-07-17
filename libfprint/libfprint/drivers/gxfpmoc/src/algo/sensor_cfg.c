#include "gxfp/algo/sensor_cfg.h"

#include <errno.h>
#include <string.h>

enum {
    GXFP_MILANL_OTP_IDX_TCODE = 0x16,
    GXFP_MILANL_OTP_IDX_TCODE_NEG = 0x17,
    GXFP_MILANL_OTP_IDX_DAC = 0x1f,

    GXFP_MILANL_DEFAULT_TCODE = 0x80,
    GXFP_MILANL_DEFAULT_FDT_DELTA = 0x15,

    GXFP_CFG_HEADER_LEN = 0x11,
    GXFP_CFG_CHECKSUM_WORDS = 0x6f,
    GXFP_CFG_CHECKSUM_OFF = 0xde,
};

static uint16_t
gxfp_le16_at(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void
gxfp_cfg_recalc_checksum(uint8_t *cfg,
                         size_t cfg_len)
{
    int16_t sum = (int16_t)0xa5a5;
    size_t i;

    if (!cfg || cfg_len < GXFP_MILANL_CFG_LEN)
        return;

    for (i = 0; i < GXFP_CFG_CHECKSUM_WORDS; i++) {
        size_t off = i * 2u;
        int16_t w = (int16_t)gxfp_le16_at(cfg + off);
        sum = (int16_t)(sum + w);
    }

    sum = (int16_t)(-sum);
    cfg[GXFP_CFG_CHECKSUM_OFF] = (uint8_t)(sum & 0xff);
    cfg[GXFP_CFG_CHECKSUM_OFF + 1] = (uint8_t)(((uint16_t)sum >> 8) & 0xff);
}

static int
gxfp_cfg_patch_reg(uint8_t *cfg,
                   size_t cfg_len,
                   uint16_t reg,
                   uint16_t val,
                   uint8_t section_idx,
                   uint8_t write_mode)
{
    uint8_t section_off;
    uint8_t section_len;
    uint8_t step;

    if (!cfg || cfg_len != GXFP_MILANL_CFG_LEN)
        return -EINVAL;
    if (section_idx > 7)
        return -EINVAL;

    section_off = cfg[1u + ((size_t)section_idx * 2u)];
    section_len = cfg[2u + ((size_t)section_idx * 2u)];
    if ((size_t)section_off + (size_t)section_len > cfg_len)
        return -EBADMSG;

    for (step = 0; step < section_len; step = (uint8_t)(step + 4u)) {
        size_t off = (size_t)section_off + (size_t)step;
        uint16_t cur;

        if (off + 4u > cfg_len)
            return -EBADMSG;

        cur = gxfp_le16_at(cfg + off);
        if (cur != reg)
            continue;

        if (write_mode == 0 || write_mode == 1)
            cfg[off + 2u] = (uint8_t)(val & 0xffu);
        if (write_mode == 0 || write_mode == 2)
            cfg[off + 3u] = (uint8_t)((val >> 8) & 0xffu);

        gxfp_cfg_recalc_checksum(cfg, cfg_len);
        return 0;
    }

    return -ENOENT;
}

static const uint8_t gxfp_milanl_cfg_template[GXFP_MILANL_CFG_LEN] = {
    0x30, 0x11, 0x64, 0x75, 0x00, 0x75, 0x2c, 0xa1, 0x1c, 0xbd, 0x18, 0xd5, 0x00, 0xd5, 0x00, 0xd5,
    0x00, 0xba, 0x00, 0x00, 0x80, 0xca, 0x00, 0x06, 0x00, 0x84, 0x00, 0xbe, 0xb2, 0x86, 0x00, 0xc5,
    0xb9, 0x88, 0x00, 0xb5, 0xad, 0x8a, 0x00, 0x9d, 0x95, 0x8c, 0x00, 0x00, 0xbe, 0x8e, 0x00, 0x00,
    0xc5, 0x90, 0x00, 0x00, 0xb5, 0x92, 0x00, 0x00, 0x9d, 0x94, 0x00, 0x00, 0xaf, 0x96, 0x00, 0x00,
    0xbf, 0x98, 0x00, 0x00, 0xb6, 0x9a, 0x00, 0x00, 0xa7, 0xd2, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00,
    0x00, 0xd6, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x12, 0x00, 0x03, 0x04, 0xd0, 0x00, 0x00,
    0x00, 0x70, 0x00, 0x00, 0x00, 0x72, 0x00, 0x78, 0x56, 0x74, 0x00, 0x34, 0x12, 0x20, 0x00, 0x10,
    0x40, 0x20, 0x02, 0x08, 0x10, 0x2a, 0x01, 0x82, 0x03, 0x22, 0x00, 0x01, 0x20, 0x24, 0x00, 0x14,
    0x00, 0x80, 0x00, 0x01, 0x04, 0x5c, 0x00, 0x00, 0x01, 0x56, 0x00, 0x0c, 0x24, 0x58, 0x00, 0x05,
    0x00, 0x32, 0x00, 0x08, 0x02, 0x66, 0x00, 0x00, 0x02, 0x7c, 0x00, 0x00, 0x38, 0x82, 0x00, 0x80,
    0x15, 0x2a, 0x01, 0x08, 0x00, 0x5c, 0x00, 0x80, 0x00, 0x54, 0x00, 0x00, 0x01, 0x62, 0x00, 0x38,
    0x04, 0x64, 0x00, 0x10, 0x00, 0x66, 0x00, 0x00, 0x02, 0x7c, 0x00, 0x01, 0x38, 0x2a, 0x01, 0x08,
    0x00, 0x5c, 0x00, 0x80, 0x00, 0x52, 0x00, 0x08, 0x00, 0x54, 0x00, 0x00, 0x01, 0x66, 0x00, 0x00,
    0x02, 0x7c, 0x00, 0x01, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int gxfp_milanl_parse_otp(const uint8_t *otp,
                          size_t otp_len,
                          struct gxfp_milanl_otp_cfg *out)
{
    uint8_t raw_tcode;

    if (!otp || !out)
        return -EINVAL;
    if (otp_len <= GXFP_MILANL_OTP_IDX_DAC)
        return -EBADMSG;

    memset(out, 0, sizeof(*out));
    out->tcode = GXFP_MILANL_DEFAULT_TCODE;
    out->fdt_delta = GXFP_MILANL_DEFAULT_FDT_DELTA;

    out->dac = (uint16_t)(((uint16_t)otp[GXFP_MILANL_OTP_IDX_DAC] << 4) | 0x0008u);
    out->has_dac = 1;

    raw_tcode = otp[GXFP_MILANL_OTP_IDX_TCODE];
    if (raw_tcode != 0 && (uint8_t)(raw_tcode + otp[GXFP_MILANL_OTP_IDX_TCODE_NEG]) == 0xffu) {
        uint16_t tcode;
        uint32_t fdt_delta;

        tcode = (uint16_t)((((uint16_t)(raw_tcode >> 4)) + 1u) * 0x10u);
        if (tcode != 0) {
            fdt_delta = (((((uint32_t)(raw_tcode & 0x0fu) + 2u) * 0x6400u) / (uint32_t)tcode) / 3u) >> 4;
            out->tcode = tcode;
            out->fdt_delta = (uint16_t)(fdt_delta & 0xffffu);
            out->has_tcode_delta = 1;
        }
    }

    return 0;
}

int gxfp_milanl_prepare_cfg_blob(const uint8_t *template_data,
                                 size_t template_len,
                                 uint8_t *out_cfg,
                                 size_t out_cfg_cap)
{
    if (!template_data || !out_cfg)
        return -EINVAL;
    if (template_len != GXFP_MILANL_CFG_LEN)
        return -EINVAL;
    if (out_cfg_cap < template_len)
        return -EMSGSIZE;

    memcpy(out_cfg, template_data, template_len);
    return (int)template_len;
}

int gxfp_milanl_apply_otp_patch(uint8_t *cfg,
                                size_t cfg_len,
                                const struct gxfp_milanl_otp_cfg *otp_cfg)
{
    int r;

    if (!cfg || !otp_cfg)
        return -EINVAL;
    if (cfg_len != GXFP_MILANL_CFG_LEN)
        return -EINVAL;

    r = gxfp_cfg_patch_reg(cfg, cfg_len, 0x0220u, otp_cfg->dac, 0, 0);
    if (r < 0)
        return r;

    if (otp_cfg->has_tcode_delta) {
        r = gxfp_cfg_patch_reg(cfg, cfg_len, 0x005cu, otp_cfg->tcode, 4, 0);
        if (r < 0)
            return r;

        r = gxfp_cfg_patch_reg(cfg,
                               cfg_len,
                               0x0082u,
                               (uint16_t)(otp_cfg->fdt_delta << 8),
                               2,
                               2);
        if (r < 0)
            return r;
    }

    return 0;
}

int gxfp_milanl_apply_default_patch(uint8_t *cfg,
                                    size_t cfg_len)
{
    struct gxfp_milanl_otp_cfg defaults;

    if (!cfg)
        return -EINVAL;

    memset(&defaults, 0, sizeof(defaults));
    defaults.tcode = GXFP_MILANL_DEFAULT_TCODE;
    defaults.fdt_delta = GXFP_MILANL_DEFAULT_FDT_DELTA;
    defaults.dac = 0x0008u;
    defaults.has_dac = 1;
    defaults.has_tcode_delta = 1;

    return gxfp_milanl_apply_otp_patch(cfg, cfg_len, &defaults);
}

int gxfp_milanl_get_default_cfg_template(const uint8_t **out_template,
                                         size_t *out_len)
{
    if (!out_template || !out_len)
        return -EINVAL;

    *out_template = gxfp_milanl_cfg_template;
    *out_len = sizeof(gxfp_milanl_cfg_template);
    return 0;
}
