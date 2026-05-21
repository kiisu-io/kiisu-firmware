#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define U2F_CERT_GEN_MAX_DER_SIZE 384u
#define U2F_CERT_GEN_PRIV_KEY_SIZE 32u

/**
 * Generate a self-signed ECDSA P-256 X.509 certificate suitable for FIDO U2F
 * self-attestation. The caller is responsible for persisting the resulting
 * DER and private key.
 *
 * @param cert_der        Output buffer for DER-encoded certificate. Must hold
 *                        at least U2F_CERT_GEN_MAX_DER_SIZE bytes.
 * @param cert_buf_size   Size of cert_der buffer.
 * @param cert_len_out    Number of DER bytes actually written.
 * @param priv_key_out    Output buffer for 32-byte raw P-256 private scalar.
 * @return                true on success, false on any cryptographic failure.
 */
bool u2f_cert_gen_self_signed(
    uint8_t* cert_der,
    size_t cert_buf_size,
    size_t* cert_len_out,
    uint8_t* priv_key_out);

#ifdef __cplusplus
}
#endif
