#include "u2f_cert_gen.h"

#include <furi_hal_random.h>

#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>
#include <mbedtls/bignum.h>

#define TAG "U2fCertGen"

#define U2F_CERT_GEN_SERIAL_LEN 8u
#define U2F_CERT_GEN_PUB_POINT_LEN 65u
#define U2F_CERT_GEN_SHA256_LEN 32u

static const char u2f_cert_gen_cn[] = "Kiisu U2F Token";
static const char u2f_cert_gen_not_before[] = "200101000000Z";
static const char u2f_cert_gen_not_after[] = "491231235959Z";

/* DER-encoded OIDs as full TLV (tag 0x06, length, value). */
static const uint8_t oid_ec_public_key[] =
    {0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};
static const uint8_t oid_prime256v1[] =
    {0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};
static const uint8_t oid_ecdsa_with_sha256[] =
    {0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02};
static const uint8_t oid_common_name[] = {0x06, 0x03, 0x55, 0x04, 0x03};

typedef struct {
    uint8_t* buf;
    size_t buf_size;
    size_t pos; /* DER is written backwards; pos is the start of written data */
    bool error;
} DerWriter;

static int u2f_cert_gen_rng_cb(void* ctx, unsigned char* dest, size_t size) {
    (void)ctx;
    furi_hal_random_fill_buf(dest, size);
    return 0;
}

static void der_init(DerWriter* w, uint8_t* buf, size_t buf_size) {
    w->buf = buf;
    w->buf_size = buf_size;
    w->pos = buf_size;
    w->error = false;
}

static bool der_w_raw(DerWriter* w, const uint8_t* data, size_t n) {
    if(w->error) return false;
    if(n > w->pos) {
        w->error = true;
        return false;
    }
    w->pos -= n;
    memcpy(w->buf + w->pos, data, n);
    return true;
}

static bool der_w_byte(DerWriter* w, uint8_t b) {
    if(w->error) return false;
    if(w->pos == 0) {
        w->error = true;
        return false;
    }
    w->pos -= 1;
    w->buf[w->pos] = b;
    return true;
}

static bool der_w_len(DerWriter* w, size_t len) {
    if(len < 0x80) {
        return der_w_byte(w, (uint8_t)len);
    } else if(len <= 0xFF) {
        return der_w_byte(w, (uint8_t)len) && der_w_byte(w, 0x81);
    } else if(len <= 0xFFFF) {
        return der_w_byte(w, (uint8_t)(len & 0xFF)) &&
               der_w_byte(w, (uint8_t)((len >> 8) & 0xFF)) && der_w_byte(w, 0x82);
    }
    w->error = true;
    return false;
}

static bool der_w_header(DerWriter* w, uint8_t tag, size_t content_len) {
    return der_w_len(w, content_len) && der_w_byte(w, tag);
}

/* Wrap previously-written content [content_end_pos..w->pos) with tag+length. */
static bool der_wrap(DerWriter* w, size_t content_end_pos, uint8_t tag) {
    if(w->error) return false;
    size_t content_len = content_end_pos - w->pos;
    return der_w_header(w, tag, content_len);
}

static bool der_w_integer(DerWriter* w, const uint8_t* value, size_t len) {
    if(w->error || len == 0) {
        w->error = true;
        return false;
    }
    /* Strip leading zero bytes, but keep at least one byte. */
    while(len > 1 && value[0] == 0x00) {
        value++;
        len--;
    }
    bool need_leading_zero = (value[0] & 0x80) != 0;
    if(!der_w_raw(w, value, len)) return false;
    if(need_leading_zero) {
        if(!der_w_byte(w, 0x00)) return false;
        return der_w_header(w, 0x02, len + 1);
    }
    return der_w_header(w, 0x02, len);
}

static bool der_w_distinguished_name(DerWriter* w) {
    size_t end = w->pos;
    /* UTF8String "Kiisu U2F Token" */
    const size_t cn_len = sizeof(u2f_cert_gen_cn) - 1;
    if(!der_w_raw(w, (const uint8_t*)u2f_cert_gen_cn, cn_len)) return false;
    if(!der_w_header(w, 0x0C, cn_len)) return false;
    /* OID CN */
    if(!der_w_raw(w, oid_common_name, sizeof(oid_common_name))) return false;
    /* SEQUENCE wrapping AttributeTypeAndValue */
    if(!der_wrap(w, end, 0x30)) return false;
    size_t end_set = w->pos;
    /* SET */
    (void)end_set;
    if(!der_wrap(w, end, 0x31)) return false;
    /* Outer SEQUENCE (Name) */
    return der_wrap(w, end, 0x30);
}

static bool der_w_utctime(DerWriter* w, const char* time_str) {
    const size_t len = strlen(time_str);
    if(!der_w_raw(w, (const uint8_t*)time_str, len)) return false;
    return der_w_header(w, 0x17, len);
}

static bool der_w_validity(DerWriter* w) {
    size_t end = w->pos;
    if(!der_w_utctime(w, u2f_cert_gen_not_after)) return false;
    if(!der_w_utctime(w, u2f_cert_gen_not_before)) return false;
    return der_wrap(w, end, 0x30);
}

static bool der_w_signature_algorithm(DerWriter* w) {
    size_t end = w->pos;
    if(!der_w_raw(w, oid_ecdsa_with_sha256, sizeof(oid_ecdsa_with_sha256))) return false;
    return der_wrap(w, end, 0x30);
}

static bool der_w_subject_public_key_info(DerWriter* w, const uint8_t* pub_point) {
    size_t end = w->pos;
    /* BIT STRING value: 0x00 unused bits + 0x04 || X || Y */
    if(!der_w_raw(w, pub_point, U2F_CERT_GEN_PUB_POINT_LEN)) return false;
    if(!der_w_byte(w, 0x00)) return false;
    if(!der_w_header(w, 0x03, U2F_CERT_GEN_PUB_POINT_LEN + 1)) return false;
    /* AlgorithmIdentifier { ecPublicKey, prime256v1 } */
    size_t end_alg = w->pos;
    if(!der_w_raw(w, oid_prime256v1, sizeof(oid_prime256v1))) return false;
    if(!der_w_raw(w, oid_ec_public_key, sizeof(oid_ec_public_key))) return false;
    if(!der_wrap(w, end_alg, 0x30)) return false;
    return der_wrap(w, end, 0x30);
}

static bool der_w_tbs_certificate(
    DerWriter* w,
    const uint8_t* serial,
    const uint8_t* pub_point) {
    size_t end = w->pos;

    /* SubjectPublicKeyInfo */
    if(!der_w_subject_public_key_info(w, pub_point)) return false;
    /* subject = issuer (self-signed) */
    if(!der_w_distinguished_name(w)) return false;
    /* validity */
    if(!der_w_validity(w)) return false;
    /* issuer */
    if(!der_w_distinguished_name(w)) return false;
    /* signature algorithm (inside TBS, must match outer) */
    if(!der_w_signature_algorithm(w)) return false;
    /* serial number INTEGER */
    if(!der_w_integer(w, serial, U2F_CERT_GEN_SERIAL_LEN)) return false;
    /* version [0] EXPLICIT INTEGER 2 (v3) */
    {
        size_t v_end = w->pos;
        const uint8_t two = 2;
        if(!der_w_integer(w, &two, 1)) return false;
        if(!der_wrap(w, v_end, 0xA0)) return false;
    }

    return der_wrap(w, end, 0x30);
}

static bool der_w_ecdsa_signature(DerWriter* w, const uint8_t* r, const uint8_t* s) {
    size_t end = w->pos;
    if(!der_w_integer(w, s, U2F_CERT_GEN_PRIV_KEY_SIZE)) return false;
    if(!der_w_integer(w, r, U2F_CERT_GEN_PRIV_KEY_SIZE)) return false;
    /* Outer SEQUENCE { r, s } */
    if(!der_wrap(w, end, 0x30)) return false;
    /* Wrap in BIT STRING with 0x00 unused-bits prefix */
    size_t sig_len = end - w->pos;
    if(!der_w_byte(w, 0x00)) return false;
    return der_w_header(w, 0x03, sig_len + 1);
}

#define U2F_CERT_GEN_TBS_BUF_SIZE 256u

bool u2f_cert_gen_self_signed(
    uint8_t* cert_der,
    size_t cert_buf_size,
    size_t* cert_len_out,
    uint8_t* priv_key_out) {
    furi_assert(cert_der);
    furi_assert(cert_len_out);
    furi_assert(priv_key_out);

    bool ok = false;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point pub;
    mbedtls_mpi priv;
    mbedtls_mpi sig_r;
    mbedtls_mpi sig_s;
    uint8_t* tbs_buf = NULL;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&pub);
    mbedtls_mpi_init(&priv);
    mbedtls_mpi_init(&sig_r);
    mbedtls_mpi_init(&sig_s);

    do {
        if(mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) break;

        /* Generate a valid P-256 private scalar. Retries are essentially never
         * triggered (failure rate ~2^-256) but the loop is here for completeness. */
        bool key_ok = false;
        for(uint8_t attempts = 0; attempts < 8 && !key_ok; attempts++) {
            furi_hal_random_fill_buf(priv_key_out, U2F_CERT_GEN_PRIV_KEY_SIZE);
            if(mbedtls_mpi_read_binary(&priv, priv_key_out, U2F_CERT_GEN_PRIV_KEY_SIZE) != 0)
                continue;
            if(mbedtls_ecp_check_privkey(&grp, &priv) == 0) key_ok = true;
        }
        if(!key_ok) break;

        /* Compute public point Q = priv * G. */
        if(mbedtls_ecp_mul(&grp, &pub, &priv, &grp.G, u2f_cert_gen_rng_cb, NULL) != 0) break;

        uint8_t pub_point[U2F_CERT_GEN_PUB_POINT_LEN];
        size_t pub_olen = 0;
        if(mbedtls_ecp_point_write_binary(
               &grp,
               &pub,
               MBEDTLS_ECP_PF_UNCOMPRESSED,
               &pub_olen,
               pub_point,
               sizeof(pub_point)) != 0)
            break;
        if(pub_olen != U2F_CERT_GEN_PUB_POINT_LEN) break;

        uint8_t serial[U2F_CERT_GEN_SERIAL_LEN];
        furi_hal_random_fill_buf(serial, sizeof(serial));
        /* Force positive INTEGER by clearing top bit. */
        serial[0] &= 0x7F;
        /* Avoid all-zero serial (illegal in RFC 5280). */
        if(serial[0] == 0) serial[0] = 0x01;

        /* Build TBSCertificate in a temporary buffer (it must be assembled before
         * signing, but the final certificate needs TBS to precede the signature
         * in the output buffer). */
        tbs_buf = malloc(U2F_CERT_GEN_TBS_BUF_SIZE);
        if(tbs_buf == NULL) break;

        DerWriter w_tbs;
        der_init(&w_tbs, tbs_buf, U2F_CERT_GEN_TBS_BUF_SIZE);
        if(!der_w_tbs_certificate(&w_tbs, serial, pub_point)) break;

        size_t tbs_len = U2F_CERT_GEN_TBS_BUF_SIZE - w_tbs.pos;
        const uint8_t* tbs_bytes = tbs_buf + w_tbs.pos;

        /* Hash the encoded TBSCertificate and sign it. */
        uint8_t tbs_hash[U2F_CERT_GEN_SHA256_LEN];
        if(mbedtls_sha256(tbs_bytes, tbs_len, tbs_hash, 0) != 0) break;

        if(mbedtls_ecdsa_sign(
               &grp,
               &sig_r,
               &sig_s,
               &priv,
               tbs_hash,
               sizeof(tbs_hash),
               u2f_cert_gen_rng_cb,
               NULL) != 0)
            break;

        uint8_t r_bin[U2F_CERT_GEN_PRIV_KEY_SIZE];
        uint8_t s_bin[U2F_CERT_GEN_PRIV_KEY_SIZE];
        if(mbedtls_mpi_write_binary(&sig_r, r_bin, sizeof(r_bin)) != 0) break;
        if(mbedtls_mpi_write_binary(&sig_s, s_bin, sizeof(s_bin)) != 0) break;

        /* Assemble the final certificate in cert_der, writing backwards from the
         * end of the buffer: signature, then signatureAlgorithm, then TBS, then
         * the outer SEQUENCE header. */
        DerWriter w;
        der_init(&w, cert_der, cert_buf_size);

        if(!der_w_ecdsa_signature(&w, r_bin, s_bin)) break;
        if(!der_w_signature_algorithm(&w)) break;
        if(tbs_len > w.pos) break;
        w.pos -= tbs_len;
        memcpy(cert_der + w.pos, tbs_bytes, tbs_len);
        if(!der_wrap(&w, cert_buf_size, 0x30)) break;

        if(w.error) break;

        size_t total_len = cert_buf_size - w.pos;
        /* Slide the encoded certificate to the start of the buffer. */
        memmove(cert_der, cert_der + w.pos, total_len);
        *cert_len_out = total_len;

        ok = true;
    } while(0);

    if(tbs_buf != NULL) free(tbs_buf);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&pub);
    mbedtls_mpi_free(&priv);
    mbedtls_mpi_free(&sig_r);
    mbedtls_mpi_free(&sig_s);

    return ok;
}
