#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>

bool u2f_data_check(bool cert_only);

/**
 * Generate a self-signed P-256 attestation certificate (and matching private
 * key file marked as USER_UNENCRYPTED) on the SD card, if they don't already
 * exist. The unencrypted private key will be encrypted with the device-unique
 * enclave key on first read by u2f_data_cert_key_load().
 *
 * @return true if files exist (newly created or already present) after the call.
 */
bool u2f_data_cert_generate_if_missing(void);

bool u2f_data_cert_check(void);

uint32_t u2f_data_cert_load(uint8_t* cert);

bool u2f_data_cert_key_load(uint8_t* cert_key);

bool u2f_data_key_load(uint8_t* device_key);

bool u2f_data_key_generate(uint8_t* device_key);

bool u2f_data_cnt_read(uint32_t* cnt);

bool u2f_data_cnt_write(uint32_t cnt);

#ifdef __cplusplus
}
#endif
