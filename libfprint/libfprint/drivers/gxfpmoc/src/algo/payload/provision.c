#include "gxfp/algo/payload/provision.h"

#include "gxfp/algo/common.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>

const uint8_t gxfp_payload_seed32_runtime[32] = {
    0x5c, 0xba, 0x6e, 0x25, 0x81, 0x95, 0x18, 0xde,
    0x2d, 0x53, 0xe9, 0x6d, 0xc0, 0x34, 0x7a, 0xb0,
    0xd4, 0x27, 0xd4, 0x08, 0x4b, 0xda, 0x4f, 0xae,
    0x1b, 0xff, 0x2b, 0x09, 0x11, 0x2a, 0x57, 0xe5,
};

enum {
    GXFP_PSK_LEN = 32,
    GXFP_BB010003_WB_HMAC_LEN = 32,
    GXFP_BB010003_HDR_LEN = 6,
    GXFP_BB010003_SEED16_LEN = 16,
    GXFP_BB010003_TAG_LEN = 16,
    GXFP_BB010003_PAYLOAD_LEN = GXFP_BB010003_HDR_LEN + GXFP_BB010003_SEED16_LEN + GXFP_PSK_LEN + GXFP_BB010003_TAG_LEN,
    GXFP_BB010003_WB_LEN = GXFP_BB010003_WB_HMAC_LEN + GXFP_BB010003_PAYLOAD_LEN,
};

static int hmac_sha256(const uint8_t *key,
                       size_t key_len,
                       const uint8_t *msg,
                       size_t msg_len,
                       uint8_t out[32]);

static void fill_tls_gcm_aad(uint8_t aad[16])
{
    gxfp_le32enc(aad + 0, 0xF0C12D52u);
    gxfp_le32enc(aad + 4, 0x077D5699u);
    gxfp_le32enc(aad + 8, 0xA3377FF4u);
    gxfp_le32enc(aad + 12, 0x7D42842Au);
}

static void fill_ff02_len_hdr(uint8_t hdr[GXFP_BB010003_HDR_LEN], uint32_t data_len)
{
    hdr[0] = 0x02;
    hdr[1] = 0xFF;
    gxfp_le32enc(hdr + 2, data_len);
}

static int compute_bb010003_wb_hmac(const uint8_t key_hmac[32],
                                    const uint8_t payload[GXFP_BB010003_PAYLOAD_LEN],
                                    uint8_t out_hmac[32])
{
    uint8_t hmac_msg[GXFP_BB010003_HDR_LEN + GXFP_PSK_LEN + GXFP_BB010003_TAG_LEN];

    memcpy(hmac_msg, payload, GXFP_BB010003_HDR_LEN);
    memcpy(hmac_msg + GXFP_BB010003_HDR_LEN,
           payload + GXFP_BB010003_HDR_LEN + GXFP_BB010003_SEED16_LEN,
           GXFP_PSK_LEN);
    memcpy(hmac_msg + GXFP_BB010003_HDR_LEN + GXFP_PSK_LEN,
           payload + GXFP_BB010003_HDR_LEN + GXFP_BB010003_SEED16_LEN + GXFP_PSK_LEN,
           GXFP_BB010003_TAG_LEN);

    return hmac_sha256(key_hmac, 32, hmac_msg, sizeof(hmac_msg), out_hmac);
}

static void local78_from_ctr(uint32_t ctr, uint8_t out[4])
{
    uint32_t uvar6;
    uint32_t local78;

    uvar6 = ((ctr * 0x10000u) | (ctr >> 16)) & 0xFFFFFFFFu;
    local78 = (((uvar6 >> 8) ^ (uvar6 << 8)) & 0x00FF00FFu) ^ (uvar6 << 8);
    gxfp_le32enc(out, local78);
}

static int hmac_sha256(const uint8_t *key,
                       size_t key_len,
                       const uint8_t *msg,
                       size_t msg_len,
                       uint8_t out[32])
{
    const mbedtls_md_info_t *md;
    int rc;

    md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md)
        return -EIO;

    rc = mbedtls_md_hmac(md, key, key_len, msg, msg_len, out);
    if (rc != 0)
        return -EIO;
    return 0;
}

int gxfp_payload_calc_seed16(const uint8_t *psk, size_t psk_len, uint8_t out_seed16[16])
{
    size_t prefix_len;
    uint8_t *buf;
    size_t off = 0;
    uint8_t digest[32];
    int i;

    if (!psk || psk_len == 0 || !out_seed16)
        return -EINVAL;

    prefix_len = psk_len >> 2;
    buf = (uint8_t *)malloc(6u + prefix_len + 64u);
    if (!buf)
        return -ENOMEM;

    buf[off++] = 0x02;
    buf[off++] = 0xFF;
    gxfp_le32enc(buf + off, (uint32_t)psk_len);
    off += 4;
    memcpy(buf + off, psk, prefix_len);
    off += prefix_len;
    for (i = 0; i < 16; i++) {
        gxfp_le32enc(buf + off, 3u);
        off += 4;
    }

    if (mbedtls_sha256(buf, off, digest, 0) != 0) {
        free(buf);
        return -EIO;
    }

    memcpy(out_seed16, digest, 16);
    free(buf);
    return 0;
}

int gxfp_payload_derive_key_material(const uint8_t seed32[32],
                                     uint8_t out_aes_key32[32],
                                     uint8_t out_hmac_key32[32])
{
    static const uint8_t base[] = {
        'k', 'g', 'o', 'o', 'd', 'w', 'i', 'x', 'g', '\0',
        'k', 'a', 'e', 'l', 'r', 'g', 'n', 'o', 'e', 'r', 'l', 'i', 't', 'h', 'm',
        0x00, 0x00, 0x01, 0x80
    };
    uint8_t msg[4 + sizeof(base)];
    uint8_t round1[32];
    uint8_t round2[32];
    int rc;

    if (!seed32 || !out_aes_key32 || !out_hmac_key32)
        return -EINVAL;

    local78_from_ctr(1u, msg);
    memcpy(msg + 4, base, sizeof(base));
    rc = hmac_sha256(seed32, 32, msg, sizeof(msg), round1);
    if (rc < 0)
        return rc;

    local78_from_ctr(2u, msg);
    memcpy(msg + 4, base, sizeof(base));
    rc = hmac_sha256(seed32, 32, msg, sizeof(msg), round2);
    if (rc < 0)
        return rc;

    memcpy(out_aes_key32, round1, 32);
    memcpy(out_hmac_key32, round1 + 16, 16);
    memcpy(out_hmac_key32 + 16, round2, 16);
    return 0;
}

int gxfp_payload_build_bb010003(const uint8_t psk32[32],
                                const uint8_t seed32[32],
                                uint8_t **out_wb_data,
                                size_t *out_wb_data_len,
                                uint8_t **out_inner_payload,
                                size_t *out_inner_payload_len)
{
    uint8_t key_aes[32];
    uint8_t key_hmac[32];
    uint8_t seed16[16];
    uint8_t hdr[6];
    uint8_t aad[16];
    uint8_t cipher[32];
    uint8_t tag[16];
    uint8_t hmac_out[32];
    uint8_t *payload;
    uint8_t *wb;
    mbedtls_gcm_context gcm;
    int rc;

    if (!psk32 || !seed32 || !out_wb_data || !out_wb_data_len)
        return -EINVAL;

    *out_wb_data = NULL;
    *out_wb_data_len = 0;
    if (out_inner_payload)
        *out_inner_payload = NULL;
    if (out_inner_payload_len)
        *out_inner_payload_len = 0;

    rc = gxfp_payload_derive_key_material(seed32, key_aes, key_hmac);
    if (rc < 0)
        return rc;

    rc = gxfp_payload_calc_seed16(psk32, GXFP_PSK_LEN, seed16);
    if (rc < 0)
        return rc;

    fill_ff02_len_hdr(hdr, GXFP_PSK_LEN);
    fill_tls_gcm_aad(aad);

    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key_aes, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return -EIO;
    }
    rc = mbedtls_gcm_crypt_and_tag(&gcm,
                                   MBEDTLS_GCM_ENCRYPT,
                                   GXFP_PSK_LEN,
                                   seed16,
                                   GXFP_BB010003_SEED16_LEN,
                                   aad,
                                   sizeof(aad),
                                   psk32,
                                   cipher,
                                   GXFP_BB010003_TAG_LEN,
                                   tag);
    mbedtls_gcm_free(&gcm);
    if (rc != 0)
        return -EIO;

    payload = (uint8_t *)malloc(GXFP_BB010003_PAYLOAD_LEN);
    if (!payload)
        return -ENOMEM;
    memcpy(payload, hdr, GXFP_BB010003_HDR_LEN);
    memcpy(payload + GXFP_BB010003_HDR_LEN, seed16, GXFP_BB010003_SEED16_LEN);
    memcpy(payload + GXFP_BB010003_HDR_LEN + GXFP_BB010003_SEED16_LEN, cipher, GXFP_PSK_LEN);
    memcpy(payload + GXFP_BB010003_HDR_LEN + GXFP_BB010003_SEED16_LEN + GXFP_PSK_LEN,
           tag,
           GXFP_BB010003_TAG_LEN);

    rc = compute_bb010003_wb_hmac(key_hmac, payload, hmac_out);
    if (rc < 0) {
        free(payload);
        return rc;
    }

    wb = (uint8_t *)malloc(GXFP_BB010003_WB_LEN);
    if (!wb) {
        free(payload);
        return -ENOMEM;
    }
    memcpy(wb, hmac_out, GXFP_BB010003_WB_HMAC_LEN);
    memcpy(wb + GXFP_BB010003_WB_HMAC_LEN, payload, GXFP_BB010003_PAYLOAD_LEN);

    *out_wb_data = wb;
    *out_wb_data_len = GXFP_BB010003_WB_LEN;

    if (out_inner_payload) {
        *out_inner_payload = payload;
        if (out_inner_payload_len)
            *out_inner_payload_len = GXFP_BB010003_PAYLOAD_LEN;
    } else {
        free(payload);
    }

    return 0;
}

int gxfp_payload_build_bb020003_hash_prefix(const uint8_t *bb010003_wb_data,
                                            size_t bb010003_wb_data_len,
                                            uint8_t *out_hash,
                                            size_t out_hash_len)
{
    uint8_t digest[48];

    if (!bb010003_wb_data || bb010003_wb_data_len == 0 || !out_hash)
        return -EINVAL;
    if (out_hash_len == 0 || out_hash_len > sizeof(digest))
        return -EINVAL;

    if (mbedtls_sha512(bb010003_wb_data, bb010003_wb_data_len, digest, 1) != 0)
        return -EIO;

    memcpy(out_hash, digest, out_hash_len);
    return 0;
}

int gxfp_payload_build_bb010002(const uint8_t *sealed_psk,
                                size_t sealed_psk_len,
                                const uint8_t seed8[8],
                                const uint8_t *bb010003_wb_data,
                                size_t bb010003_wb_data_len,
                                int pad4,
                                uint8_t **out_blob,
                                size_t *out_blob_len)
{
    uint32_t len_field;
    uint8_t *out;
    size_t out_len;
    size_t off = 0;

    if ((!sealed_psk && sealed_psk_len != 0) || !seed8 || !bb010003_wb_data || bb010003_wb_data_len == 0 ||
        !out_blob || !out_blob_len)
        return -EINVAL;

    if (sealed_psk_len > 0xFFFFFFFFu - 8u)
        return -EOVERFLOW;
    if (bb010003_wb_data_len > 0xFFFFFFFFu)
        return -EOVERFLOW;

    out_len = 8u + sealed_psk_len + 8u + 8u + bb010003_wb_data_len;
    if (pad4)
        out_len = (out_len + 3u) & ~3u;

    out = (uint8_t *)calloc(1, out_len);
    if (!out)
        return -ENOMEM;

    gxfp_le32enc(out + off, 0xBB010002u);
    off += 4;
    len_field = (uint32_t)(sealed_psk_len + 8u);
    gxfp_le32enc(out + off, len_field);
    off += 4;

    if (sealed_psk_len)
        memcpy(out + off, sealed_psk, sealed_psk_len);
    off += sealed_psk_len;
    memcpy(out + off, seed8, 8);
    off += 8;

    gxfp_le32enc(out + off, 0xBB010003u);
    off += 4;
    gxfp_le32enc(out + off, (uint32_t)bb010003_wb_data_len);
    off += 4;
    memcpy(out + off, bb010003_wb_data, bb010003_wb_data_len);

    *out_blob = out;
    *out_blob_len = out_len;
    return 0;
}
