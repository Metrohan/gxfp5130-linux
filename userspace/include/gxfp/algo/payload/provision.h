#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t gxfp_payload_seed32_runtime[32];

int gxfp_payload_calc_seed16(const uint8_t *psk, size_t psk_len, uint8_t out_seed16[16]);

int gxfp_payload_derive_key_material(const uint8_t seed32[32],
                                     uint8_t out_aes_key32[32],
                                     uint8_t out_hmac_key32[32]);

int gxfp_payload_build_bb010003(const uint8_t psk32[32],
                                const uint8_t seed32[32],
                                uint8_t **out_wb_data,
                                size_t *out_wb_data_len,
                                uint8_t **out_inner_payload,
                                size_t *out_inner_payload_len);

int gxfp_payload_build_bb020003_hash_prefix(const uint8_t *bb010003_wb_data,
                                            size_t bb010003_wb_data_len,
                                            uint8_t *out_hash,
                                            size_t out_hash_len);

int gxfp_payload_build_bb010002(const uint8_t *sealed_psk,
                                size_t sealed_psk_len,
                                const uint8_t seed8[8],
                                const uint8_t *bb010003_wb_data,
                                size_t bb010003_wb_data_len,
                                int pad4,
                                uint8_t **out_blob,
                                size_t *out_blob_len);

#ifdef __cplusplus
}
#endif
