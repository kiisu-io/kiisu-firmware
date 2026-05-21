#include <furi_hal_crypto.h>
#include <furi_hal_cortex.h>
#include <furi_hal_bt.h>
#include <furi_hal_random.h>
#include <furi_hal_bus.h>

#include <stm32wbxx_ll_cortex.h>
#include <furi.h>
#include <interface/patterns/ble_thread/shci/shci.h>

#define TAG "FuriHalCrypto"

#define ENCLAVE_FACTORY_KEY_SLOTS 10
#define ENCLAVE_SIGNATURE_SIZE    16

#define CRYPTO_BLK_LEN    (4 * sizeof(uint32_t))
#define CRYPTO_TIMEOUT_US (1000000)

#define CRYPTO_MODE_ENCRYPT      0U
#define CRYPTO_MODE_INIT         (AES_CR_MODE_0)
#define CRYPTO_MODE_DECRYPT      (AES_CR_MODE_1)
#define CRYPTO_MODE_DECRYPT_INIT (AES_CR_MODE_0 | AES_CR_MODE_1)

#define CRYPTO_DATATYPE_32B 0U
#define CRYPTO_KEYSIZE_256B (AES_CR_KEYSIZE)
#define CRYPTO_AES_CBC      (AES_CR_CHMOD_0)

#define CRYPTO_AES_CTR     (AES_CR_CHMOD_1)
#define CRYPTO_CTR_IV_LEN  (12U)
#define CRYPTO_CTR_CTR_LEN (4U)

#define CRYPTO_AES_GCM        (AES_CR_CHMOD_1 | AES_CR_CHMOD_0)
#define CRYPTO_GCM_IV_LEN     (12U)
#define CRYPTO_GCM_CTR_LEN    (4U)
#define CRYPTO_GCM_TAG_LEN    (16U)
#define CRYPTO_GCM_PH_INIT    (0x0U << AES_CR_GCMPH_Pos)
#define CRYPTO_GCM_PH_HEADER  (AES_CR_GCMPH_0)
#define CRYPTO_GCM_PH_PAYLOAD (AES_CR_GCMPH_1)
#define CRYPTO_GCM_PH_FINAL   (AES_CR_GCMPH_1 | AES_CR_GCMPH_0)

static FuriMutex* furi_hal_crypto_mutex = NULL;
static bool furi_hal_crypto_mode_init_done = false;

static const uint8_t enclave_signature_iv[ENCLAVE_FACTORY_KEY_SLOTS][ENCLAVE_SIGNATURE_SIZE] = {
    {0xab, 0xd3, 0x06, 0xc7, 0x30, 0x1c, 0x8f, 0x47, 0x16, 0x36, 0x96, 0x47, 0x53, 0x05, 0xe1, 0x79},
    {0x66, 0x98, 0x54, 0x5f, 0x7a, 0xe4, 0xdc, 0x3e, 0xf1, 0x2a, 0xec, 0x62, 0xfc, 0x46, 0xcc, 0xb7},
    {0x3f, 0x49, 0x07, 0x86, 0x27, 0xb6, 0x8b, 0x75, 0x5b, 0xb5, 0x94, 0xc1, 0xd2, 0x83, 0xfc, 0xe3},
    {0x6f, 0x34, 0x63, 0x10, 0xa1, 0x37, 0xe4, 0x39, 0x16, 0x26, 0x97, 0x69, 0xe9, 0xcb, 0x0f, 0xe2},
    {0x3a, 0xae, 0x80, 0x50, 0x62, 0x20, 0x64, 0x18, 0x84, 0xa6, 0xe4, 0xb2, 0xa6, 0xf9, 0x72, 0xe4},
    {0xff, 0x03, 0x3d, 0x0b, 0x27, 0xe5, 0x1c, 0x5e, 0x15, 0x91, 0x5b, 0x06, 0xcf, 0x5e, 0x1d, 0x12},
    {0x26, 0xa7, 0x44, 0xb8, 0x48, 0x9c, 0xaa, 0x25, 0x5e, 0xaf, 0x66, 0x56, 0x34, 0x81, 0xfd, 0xe7},
    {0xcb, 0x4e, 0x2a, 0xde, 0x30, 0xa1, 0xe0, 0xc1, 0x51, 0x53, 0x61, 0xb1, 0xed, 0xc6, 0xaf, 0x2d},
    {0x09, 0x65, 0xd3, 0x58, 0xdc, 0xcd, 0x43, 0x0e, 0x44, 0x04, 0xa0, 0x9a, 0x28, 0x81, 0x85, 0x19},
    {0xb7, 0xe9, 0x9a, 0xfd, 0x03, 0x33, 0x2a, 0x0e, 0x87, 0xed, 0x0f, 0x88, 0x2b, 0x01, 0xd2, 0x9a},
};

static const uint8_t enclave_signature_input[ENCLAVE_FACTORY_KEY_SLOTS][ENCLAVE_SIGNATURE_SIZE] = {
    {0xc2, 0x50, 0x16, 0xf9, 0xd9, 0xc0, 0x43, 0x65, 0x9e, 0x80, 0xb8, 0xc0, 0x06, 0x72, 0xb1, 0x97},
    {0xe0, 0xfc, 0xeb, 0x9d, 0x9c, 0xb8, 0xd9, 0xe2, 0x57, 0xfb, 0x2a, 0x82, 0x2a, 0x09, 0x7c, 0xfb},
    {0x35, 0x72, 0x7c, 0xb9, 0x50, 0x3e, 0x8d, 0xf2, 0x9e, 0x18, 0xc3, 0x81, 0xf5, 0x05, 0x81, 0x4d},
    {0xb1, 0xb0, 0xd2, 0x88, 0xc1, 0xf1, 0x7a, 0xf9, 0xa4, 0x90, 0x7f, 0x05, 0x89, 0xc1, 0xb8, 0x48},
    {0x45, 0x8b, 0xe9, 0x7c, 0x02, 0x81, 0x88, 0x58, 0x51, 0x03, 0x70, 0xee, 0xec, 0x46, 0xa3, 0xa5},
    {0xe4, 0xcc, 0xdd, 0xca, 0xcd, 0x78, 0x61, 0xf2, 0x2c, 0x41, 0x67, 0x50, 0xb0, 0x01, 0x0c, 0x4d},
    {0xd5, 0xaf, 0x71, 0xb7, 0xff, 0xed, 0x7e, 0x25, 0x3e, 0xe7, 0x85, 0x3e, 0x47, 0xf0, 0x42, 0xc6},
    {0xb3, 0x67, 0x0e, 0xba, 0x61, 0x48, 0xcd, 0x8d, 0x98, 0xb3, 0xde, 0x12, 0xfe, 0x4a, 0xc6, 0x62},
    {0x2f, 0x0d, 0x1f, 0x06, 0xbe, 0x6f, 0xd5, 0x55, 0xa2, 0xe5, 0x59, 0x06, 0xae, 0x78, 0x9b, 0x80},
    {0x0e, 0x36, 0x21, 0x6a, 0x16, 0xf2, 0xd0, 0x44, 0xc8, 0xa0, 0x11, 0xb0, 0xd1, 0xa2, 0x40, 0x14},
};

static const uint8_t enclave_signature_expected[ENCLAVE_FACTORY_KEY_SLOTS][ENCLAVE_SIGNATURE_SIZE] = {
    {0x01, 0xb7, 0x6c, 0xf4, 0xc3, 0x14, 0xbf, 0xef, 0xcc, 0xef, 0x0b, 0x3a, 0x94, 0x60, 0x23, 0x05},
    {0xb7, 0x3e, 0xa5, 0x5e, 0x5e, 0x4d, 0x23, 0x6b, 0xb5, 0x21, 0x5e, 0xd8, 0x34, 0xfc, 0xac, 0x77},
    {0x70, 0x34, 0x39, 0xf8, 0x5e, 0xac, 0xc6, 0x89, 0xb1, 0x81, 0x4f, 0xfb, 0xd3, 0x52, 0xf3, 0xe6},
    {0xb0, 0x18, 0x81, 0xd5, 0x9a, 0x44, 0x34, 0x0b, 0xa6, 0x6a, 0x71, 0xc4, 0x97, 0x68, 0x50, 0x88},
    {0x7f, 0xcc, 0xb0, 0x2b, 0x1f, 0x98, 0xb2, 0x52, 0xa7, 0x8e, 0x0e, 0xed, 0xa7, 0xf7, 0x11, 0x86},
    {0x7d, 0xb0, 0x29, 0x44, 0x68, 0xec, 0x63, 0xb4, 0x71, 0xd2, 0x2f, 0x0b, 0x0e, 0x7e, 0xf3, 0xf7},
    {0xc1, 0xd3, 0xb0, 0xf3, 0x3d, 0x3a, 0x38, 0x52, 0x7c, 0x95, 0x66, 0x7b, 0x96, 0x24, 0xff, 0xec},
    {0xc6, 0x9f, 0x62, 0x67, 0x03, 0x3e, 0xba, 0x2f, 0x09, 0x5c, 0x84, 0xea, 0x2f, 0xfc, 0x77, 0xee},
    {0x01, 0xf4, 0xa4, 0xe7, 0x76, 0xef, 0xe1, 0xb8, 0x1f, 0x34, 0xcb, 0xff, 0x78, 0xb4, 0xf4, 0x14},
    {0xe6, 0x5d, 0x88, 0xf1, 0x6a, 0x8d, 0x94, 0xca, 0x82, 0x2e, 0xdf, 0x58, 0x70, 0x4e, 0xd9, 0x2e},
};

void furi_hal_crypto_init(void) {
    furi_hal_crypto_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    FURI_LOG_I(TAG, "Init OK");
}

static bool furi_hal_crypto_generate_unique_keys(uint8_t start_slot, uint8_t end_slot) {
    FuriHalCryptoKey key;
    uint8_t key_data[32];
    FURI_LOG_I(TAG, "Generating keys %u..%u", start_slot, end_slot);
    for(uint8_t slot = start_slot; slot <= end_slot; slot++) {
        key.type = FuriHalCryptoKeyTypeSimple;
        key.size = FuriHalCryptoKeySize256;
        key.data = key_data;
        furi_hal_random_fill_buf(key_data, 32);
        if(!furi_hal_crypto_enclave_store_key(&key, &slot)) {
            FURI_LOG_E(TAG, "Error writing key to slot %u", slot);
            return false;
        }
    }
    return true;
}

bool furi_hal_crypto_enclave_ensure_key(uint8_t key_slot) {
    uint8_t keys_nb = 0;
    uint8_t valid_keys_nb = 0;
    uint8_t last_valid_slot = ENCLAVE_FACTORY_KEY_SLOTS;
    uint8_t empty_iv[16] = {0};
    furi_hal_crypto_enclave_verify(&keys_nb, &valid_keys_nb);
    if(key_slot <= ENCLAVE_FACTORY_KEY_SLOTS) { // It's a factory key
        if(key_slot > keys_nb) return false;
    } else { // Unique key
        if(keys_nb < ENCLAVE_FACTORY_KEY_SLOTS) // Some factory keys are missing
            return false;
        for(uint8_t i = key_slot; i > ENCLAVE_FACTORY_KEY_SLOTS; i--) {
            if(furi_hal_crypto_enclave_load_key(i, empty_iv)) {
                last_valid_slot = i;
                furi_hal_crypto_enclave_unload_key(i);
                break;
            }
        }
        if(last_valid_slot == key_slot)
            return true;
        else // Generate missing unique keys
            return furi_hal_crypto_generate_unique_keys(last_valid_slot + 1, key_slot);
    }
    return true;
}

bool furi_hal_crypto_enclave_verify(uint8_t* keys_nb, uint8_t* valid_keys_nb) {
    furi_check(keys_nb);
    furi_check(valid_keys_nb);
    uint8_t keys = 0;
    uint8_t keys_valid = 0;
    uint8_t buffer[ENCLAVE_SIGNATURE_SIZE];
    for(size_t key_slot = 0; key_slot < ENCLAVE_FACTORY_KEY_SLOTS; key_slot++) {
        if(furi_hal_crypto_enclave_load_key(key_slot + 1, enclave_signature_iv[key_slot])) {
            keys++;
            if(furi_hal_crypto_encrypt(
                   enclave_signature_input[key_slot], buffer, ENCLAVE_SIGNATURE_SIZE)) {
                keys_valid +=
                    memcmp(buffer, enclave_signature_expected[key_slot], ENCLAVE_SIGNATURE_SIZE) ==
                    0;
            }
            furi_hal_crypto_enclave_unload_key(key_slot + 1);
        }
    }
    *keys_nb = keys;
    *valid_keys_nb = keys_valid;
    if(*valid_keys_nb == ENCLAVE_FACTORY_KEY_SLOTS)
        return true;
    else
        return false;
}

bool furi_hal_crypto_enclave_store_key(FuriHalCryptoKey* key, uint8_t* slot) {
    furi_check(key);
    furi_check(slot);

    furi_check(furi_mutex_acquire(furi_hal_crypto_mutex, FuriWaitForever) == FuriStatusOk);

    if(!furi_hal_bt_is_alive()) {
        return false;
    }

    SHCI_C2_FUS_StoreUsrKey_Cmd_Param_t pParam;
    size_t key_data_size = 0;

    if(key->type == FuriHalCryptoKeyTypeMaster) {
        pParam.KeyType = KEYTYPE_MASTER;
    } else if(key->type == FuriHalCryptoKeyTypeSimple) {
        pParam.KeyType = KEYTYPE_SIMPLE;
    } else if(key->type == FuriHalCryptoKeyTypeEncrypted) {
        pParam.KeyType = KEYTYPE_ENCRYPTED;
        key_data_size += 12;
    } else {
        furi_crash("Incorrect key type");
    }

    if(key->size == FuriHalCryptoKeySize128) {
        pParam.KeySize = KEYSIZE_16;
        key_data_size += 16;
    } else if(key->size == FuriHalCryptoKeySize256) {
        pParam.KeySize = KEYSIZE_32;
        key_data_size += 32;
    } else {
        furi_crash("Incorrect key size");
    }

    memcpy(pParam.KeyData, key->data, key_data_size);

    SHCI_CmdStatus_t shci_state = SHCI_C2_FUS_StoreUsrKey(&pParam, slot);
    furi_check(furi_mutex_release(furi_hal_crypto_mutex) == FuriStatusOk);
    return shci_state == SHCI_Success;
}

static void crypto_key_init(uint32_t* key, uint32_t* iv) {
    CLEAR_BIT(AES1->CR, AES_CR_EN);
    MODIFY_REG(
        AES1->CR,
        AES_CR_DATATYPE | AES_CR_KEYSIZE | AES_CR_CHMOD,
        CRYPTO_DATATYPE_32B | CRYPTO_KEYSIZE_256B | CRYPTO_AES_CBC);

    if(key != NULL) {
        AES1->KEYR7 = key[0];
        AES1->KEYR6 = key[1];
        AES1->KEYR5 = key[2];
        AES1->KEYR4 = key[3];
        AES1->KEYR3 = key[4];
        AES1->KEYR2 = key[5];
        AES1->KEYR1 = key[6];
        AES1->KEYR0 = key[7];
    }

    AES1->IVR3 = iv[0];
    AES1->IVR2 = iv[1];
    AES1->IVR1 = iv[2];
    AES1->IVR0 = iv[3];
}

static bool furi_hal_crypto_wait_flag(uint32_t flag) {
    FuriHalCortexTimer timer = furi_hal_cortex_timer_get(CRYPTO_TIMEOUT_US);
    while(!READ_BIT(AES1->SR, flag)) {
        if(furi_hal_cortex_timer_is_expired(timer)) {
            return false;
        }
    }
    return true;
}

static bool crypto_process_block(uint32_t* in, uint32_t* out, uint8_t blk_len) {
    furi_check((blk_len <= 4) && (blk_len > 0));

    for(uint8_t i = 0; i < 4; i++) {
        if(i < blk_len) {
            AES1->DINR = in[i];
        } else {
            AES1->DINR = 0;
        }
    }

    if(!furi_hal_crypto_wait_flag(AES_SR_CCF)) {
        return false;
    }

    SET_BIT(AES1->CR, AES_CR_CCFC);

    uint32_t out_temp[4];
    for(uint8_t i = 0; i < 4; i++) {
        out_temp[i] = AES1->DOUTR;
    }

    memcpy(out, out_temp, blk_len * sizeof(uint32_t));
    return true;
}

bool furi_hal_crypto_enclave_load_key(uint8_t slot, const uint8_t* iv) {
    furi_check(slot > 0 && slot <= 100);
    furi_check(furi_hal_crypto_mutex);
    furi_check(furi_mutex_acquire(furi_hal_crypto_mutex, FuriWaitForever) == FuriStatusOk);

    furi_hal_bus_enable(FuriHalBusAES1);

    bool success = false;

    furi_hal_bt_lock_core2();

    do {
        if(!furi_hal_bt_is_alive()) {
            break;
        }

        furi_hal_crypto_mode_init_done = false;
        crypto_key_init(NULL, (uint32_t*)iv);

        if(SHCI_C2_FUS_LoadUsrKey(slot) == SHCI_Success) {
            success = true;
        } else {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            furi_check(furi_mutex_release(furi_hal_crypto_mutex) == FuriStatusOk);
        }

    } while(false);

    furi_hal_bt_unlock_core2();
    return success;
}

bool furi_hal_crypto_enclave_unload_key(uint8_t slot) {
    furi_hal_bt_lock_core2();

    bool success = false;

    do {
        if(!furi_hal_bt_is_alive()) {
            break;
        }

        CLEAR_BIT(AES1->CR, AES_CR_EN);

        SHCI_CmdStatus_t shci_state = SHCI_C2_FUS_UnloadUsrKey(slot);

        furi_hal_bus_disable(FuriHalBusAES1);

        furi_check(furi_mutex_release(furi_hal_crypto_mutex) == FuriStatusOk);

        success = (shci_state == SHCI_Success);
    } while(false);

    furi_hal_bt_unlock_core2();
    return success;
}

bool furi_hal_crypto_load_key(const uint8_t* key, const uint8_t* iv) {
    furi_check(furi_hal_crypto_mutex);
    furi_check(furi_mutex_acquire(furi_hal_crypto_mutex, FuriWaitForever) == FuriStatusOk);

    furi_hal_bus_enable(FuriHalBusAES1);

    furi_hal_crypto_mode_init_done = false;
    crypto_key_init((uint32_t*)key, (uint32_t*)iv);

    return true;
}

bool furi_hal_crypto_unload_key(void) {
    CLEAR_BIT(AES1->CR, AES_CR_EN);

    furi_hal_bus_disable(FuriHalBusAES1);

    furi_check(furi_mutex_release(furi_hal_crypto_mutex) == FuriStatusOk);
    return true;
}

bool furi_hal_crypto_encrypt(const uint8_t* input, uint8_t* output, size_t size) {
    bool state = false;

    SET_BIT(AES1->CR, AES_CR_EN);

    MODIFY_REG(AES1->CR, AES_CR_MODE, CRYPTO_MODE_ENCRYPT);

    for(size_t i = 0; i < size; i += CRYPTO_BLK_LEN) {
        size_t blk_len = size - i;
        if(blk_len > CRYPTO_BLK_LEN) {
            blk_len = CRYPTO_BLK_LEN;
        }
        state = crypto_process_block((uint32_t*)&input[i], (uint32_t*)&output[i], blk_len / 4);
        if(state == false) {
            break;
        }
    }

    CLEAR_BIT(AES1->CR, AES_CR_EN);

    return state;
}

bool furi_hal_crypto_decrypt(const uint8_t* input, uint8_t* output, size_t size) {
    bool state = false;

    if(!furi_hal_crypto_mode_init_done) {
        MODIFY_REG(AES1->CR, AES_CR_MODE, CRYPTO_MODE_INIT);

        SET_BIT(AES1->CR, AES_CR_EN);

        if(!furi_hal_crypto_wait_flag(AES_SR_CCF)) {
            return false;
        }

        SET_BIT(AES1->CR, AES_CR_CCFC);

        furi_hal_crypto_mode_init_done = true;
    }

    MODIFY_REG(AES1->CR, AES_CR_MODE, CRYPTO_MODE_DECRYPT);
    SET_BIT(AES1->CR, AES_CR_EN);

    for(size_t i = 0; i < size; i += CRYPTO_BLK_LEN) {
        size_t blk_len = size - i;
        if(blk_len > CRYPTO_BLK_LEN) {
            blk_len = CRYPTO_BLK_LEN;
        }
        state = crypto_process_block((uint32_t*)&input[i], (uint32_t*)&output[i], blk_len / 4);
        if(state == false) {
            break;
        }
    }

    CLEAR_BIT(AES1->CR, AES_CR_EN);

    return state;
}

static void crypto_key_init_bswap(uint32_t* key, uint32_t* iv, uint32_t chaining_mode) {
    CLEAR_BIT(AES1->CR, AES_CR_EN);
    MODIFY_REG(
        AES1->CR,
        AES_CR_DATATYPE | AES_CR_KEYSIZE | AES_CR_CHMOD,
        CRYPTO_DATATYPE_32B | CRYPTO_KEYSIZE_256B | chaining_mode);

    if(key != NULL) {
        AES1->KEYR7 = __builtin_bswap32(key[0]);
        AES1->KEYR6 = __builtin_bswap32(key[1]);
        AES1->KEYR5 = __builtin_bswap32(key[2]);
        AES1->KEYR4 = __builtin_bswap32(key[3]);
        AES1->KEYR3 = __builtin_bswap32(key[4]);
        AES1->KEYR2 = __builtin_bswap32(key[5]);
        AES1->KEYR1 = __builtin_bswap32(key[6]);
        AES1->KEYR0 = __builtin_bswap32(key[7]);
    }

    AES1->IVR3 = __builtin_bswap32(iv[0]);
    AES1->IVR2 = __builtin_bswap32(iv[1]);
    AES1->IVR1 = __builtin_bswap32(iv[2]);
    AES1->IVR0 = __builtin_bswap32(iv[3]);
}

static bool
    furi_hal_crypto_load_key_bswap(const uint8_t* key, const uint8_t* iv, uint32_t chaining_mode) {
    furi_check(furi_hal_crypto_mutex);
    furi_check(furi_mutex_acquire(furi_hal_crypto_mutex, FuriWaitForever) == FuriStatusOk);

    furi_hal_bus_enable(FuriHalBusAES1);

    crypto_key_init_bswap((uint32_t*)key, (uint32_t*)iv, chaining_mode);

    return true;
}

static bool wait_for_crypto(void) {
    if(!furi_hal_crypto_wait_flag(AES_SR_CCF)) {
        return false;
    }

    SET_BIT(AES1->CR, AES_CR_CCFC);

    return true;
}

static bool furi_hal_crypto_process_block_bswap(const uint8_t* in, uint8_t* out, size_t bytes) {
    uint32_t block[CRYPTO_BLK_LEN / 4];
    memset(block, 0, sizeof(block));

    memcpy(block, in, bytes);

    block[0] = __builtin_bswap32(block[0]);
    block[1] = __builtin_bswap32(block[1]);
    block[2] = __builtin_bswap32(block[2]);
    block[3] = __builtin_bswap32(block[3]);

    if(!crypto_process_block(block, block, CRYPTO_BLK_LEN / 4)) {
        return false;
    }

    block[0] = __builtin_bswap32(block[0]);
    block[1] = __builtin_bswap32(block[1]);
    block[2] = __builtin_bswap32(block[2]);
    block[3] = __builtin_bswap32(block[3]);

    memcpy(out, block, bytes);

    return true;
}

static bool furi_hal_crypto_process_block_no_read_bswap(const uint8_t* in, size_t bytes) {
    uint32_t block[CRYPTO_BLK_LEN / 4];
    memset(block, 0, sizeof(block));

    memcpy(block, in, bytes);

    AES1->DINR = __builtin_bswap32(block[0]);
    AES1->DINR = __builtin_bswap32(block[1]);
    AES1->DINR = __builtin_bswap32(block[2]);
    AES1->DINR = __builtin_bswap32(block[3]);

    return wait_for_crypto();
}

static void furi_hal_crypto_ctr_prep_iv(uint8_t* iv) {
    /* append counter to IV */
    iv[CRYPTO_CTR_IV_LEN] = 0;
    iv[CRYPTO_CTR_IV_LEN + 1] = 0;
    iv[CRYPTO_CTR_IV_LEN + 2] = 0;
    iv[CRYPTO_CTR_IV_LEN + 3] = 1;
}

static bool furi_hal_crypto_ctr_payload(const uint8_t* input, uint8_t* output, size_t length) {
    SET_BIT(AES1->CR, AES_CR_EN);
    MODIFY_REG(AES1->CR, AES_CR_MODE, CRYPTO_MODE_ENCRYPT);

    size_t last_block_bytes = length % CRYPTO_BLK_LEN;

    size_t i;
    for(i = 0; i < length - last_block_bytes; i += CRYPTO_BLK_LEN) {
        if(!furi_hal_crypto_process_block_bswap(&input[i], &output[i], CRYPTO_BLK_LEN)) {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            return false;
        }
    }

    if(last_block_bytes > 0) {
        if(!furi_hal_crypto_process_block_bswap(&input[i], &output[i], last_block_bytes)) {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            return false;
        }
    }

    CLEAR_BIT(AES1->CR, AES_CR_EN);
    return true;
}

bool furi_hal_crypto_ctr(
    const uint8_t* key,
    const uint8_t* iv,
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    /* prepare IV and counter */
    uint8_t iv_and_counter[CRYPTO_CTR_IV_LEN + CRYPTO_CTR_CTR_LEN];
    memcpy(iv_and_counter, iv, CRYPTO_CTR_IV_LEN); //-V1086
    furi_hal_crypto_ctr_prep_iv(iv_and_counter);

    /* load key and IV and set the mode to CTR */
    if(!furi_hal_crypto_load_key_bswap(key, iv_and_counter, CRYPTO_AES_CTR)) {
        furi_hal_crypto_unload_key();
        return false;
    }

    /* process the input and write to output */
    bool state = furi_hal_crypto_ctr_payload(input, output, length);

    furi_hal_crypto_unload_key();

    return state;
}

static void furi_hal_crypto_gcm_prep_iv(uint8_t* iv) {
    /* append counter to IV */
    iv[CRYPTO_GCM_IV_LEN] = 0;
    iv[CRYPTO_GCM_IV_LEN + 1] = 0;
    iv[CRYPTO_GCM_IV_LEN + 2] = 0;
    iv[CRYPTO_GCM_IV_LEN + 3] = 2;
}

static bool furi_hal_crypto_gcm_init(bool decrypt) {
    /* GCM init phase */

    MODIFY_REG(AES1->CR, AES_CR_GCMPH, CRYPTO_GCM_PH_INIT);
    if(decrypt) {
        MODIFY_REG(AES1->CR, AES_CR_MODE, CRYPTO_MODE_DECRYPT);
    } else {
        MODIFY_REG(AES1->CR, AES_CR_MODE, CRYPTO_MODE_ENCRYPT);
    }

    SET_BIT(AES1->CR, AES_CR_EN);

    if(!wait_for_crypto()) {
        CLEAR_BIT(AES1->CR, AES_CR_EN);
        return false;
    }

    return true;
}

static bool furi_hal_crypto_gcm_header(const uint8_t* aad, size_t aad_length) {
    /* GCM header phase */

    MODIFY_REG(AES1->CR, AES_CR_GCMPH, CRYPTO_GCM_PH_HEADER);
    SET_BIT(AES1->CR, AES_CR_EN);

    size_t last_block_bytes = aad_length % CRYPTO_BLK_LEN;

    size_t i;
    for(i = 0; i < aad_length - last_block_bytes; i += CRYPTO_BLK_LEN) {
        if(!furi_hal_crypto_process_block_no_read_bswap(&aad[i], CRYPTO_BLK_LEN)) {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            return false;
        }
    }

    if(last_block_bytes > 0) {
        if(!furi_hal_crypto_process_block_no_read_bswap(&aad[i], last_block_bytes)) {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            return false;
        }
    }

    return true;
}

static bool furi_hal_crypto_gcm_payload(
    const uint8_t* input,
    uint8_t* output,
    size_t length,
    bool decrypt) {
    /* GCM payload phase */

    MODIFY_REG(AES1->CR, AES_CR_GCMPH, CRYPTO_GCM_PH_PAYLOAD);
    SET_BIT(AES1->CR, AES_CR_EN);

    size_t last_block_bytes = length % CRYPTO_BLK_LEN;

    size_t i;
    for(i = 0; i < length - last_block_bytes; i += CRYPTO_BLK_LEN) {
        if(!furi_hal_crypto_process_block_bswap(&input[i], &output[i], CRYPTO_BLK_LEN)) {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            return false;
        }
    }

    if(last_block_bytes > 0) {
        if(!decrypt) {
            MODIFY_REG(
                AES1->CR, AES_CR_NPBLB, (CRYPTO_BLK_LEN - last_block_bytes) << AES_CR_NPBLB_Pos);
        }
        if(!furi_hal_crypto_process_block_bswap(&input[i], &output[i], last_block_bytes)) {
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            return false;
        }
    }

    return true;
}

static bool furi_hal_crypto_gcm_finish(size_t aad_length, size_t payload_length, uint8_t* tag) {
    /* GCM final phase */

    MODIFY_REG(AES1->CR, AES_CR_GCMPH, CRYPTO_GCM_PH_FINAL);

    uint32_t last_block[CRYPTO_BLK_LEN / 4];
    memset(last_block, 0, sizeof(last_block));
    last_block[1] = __builtin_bswap32((uint32_t)(aad_length * 8));
    last_block[3] = __builtin_bswap32((uint32_t)(payload_length * 8));

    if(!furi_hal_crypto_process_block_bswap((uint8_t*)&last_block[0], tag, CRYPTO_BLK_LEN)) {
        CLEAR_BIT(AES1->CR, AES_CR_EN);
        return false;
    }

    return true;
}

static bool furi_hal_crypto_gcm_compare_tag(const uint8_t* tag1, const uint8_t* tag2) {
    uint8_t diff = 0;

    size_t i;
    for(i = 0; i < CRYPTO_GCM_TAG_LEN; i++) {
        diff |= tag1[i] ^ tag2[i];
    }

    return diff == 0;
}

bool furi_hal_crypto_gcm(
    const uint8_t* key,
    const uint8_t* iv,
    const uint8_t* aad,
    size_t aad_length,
    const uint8_t* input,
    uint8_t* output,
    size_t length,
    uint8_t* tag,
    bool decrypt) {
    /* GCM init phase */

    /* prepare IV and counter */
    uint8_t iv_and_counter[CRYPTO_GCM_IV_LEN + CRYPTO_GCM_CTR_LEN];
    memcpy(iv_and_counter, iv, CRYPTO_GCM_IV_LEN); //-V1086
    furi_hal_crypto_gcm_prep_iv(iv_and_counter);

    /* load key and IV and set the mode to CTR */
    if(!furi_hal_crypto_load_key_bswap(key, iv_and_counter, CRYPTO_AES_GCM)) {
        furi_hal_crypto_unload_key();
        return false;
    }

    if(!furi_hal_crypto_gcm_init(decrypt)) {
        furi_hal_crypto_unload_key();
        return false;
    }

    /* GCM header phase */

    if(aad_length > 0) {
        if(!furi_hal_crypto_gcm_header(aad, aad_length)) {
            furi_hal_crypto_unload_key();
            return false;
        }
    }

    /* GCM payload phase */

    if(!furi_hal_crypto_gcm_payload(input, output, length, decrypt)) {
        furi_hal_crypto_unload_key();
        return false;
    }

    /* GCM final phase */

    if(!furi_hal_crypto_gcm_finish(aad_length, length, tag)) {
        furi_hal_crypto_unload_key();
        return false;
    }

    furi_hal_crypto_unload_key();
    return true;
}

FuriHalCryptoGCMState furi_hal_crypto_gcm_encrypt_and_tag(
    const uint8_t* key,
    const uint8_t* iv,
    const uint8_t* aad,
    size_t aad_length,
    const uint8_t* input,
    uint8_t* output,
    size_t length,
    uint8_t* tag) {
    if(!furi_hal_crypto_gcm(key, iv, aad, aad_length, input, output, length, tag, false)) {
        memset(output, 0, length);
        memset(tag, 0, CRYPTO_GCM_TAG_LEN);
        return FuriHalCryptoGCMStateError;
    }

    return FuriHalCryptoGCMStateOk;
}

FuriHalCryptoGCMState furi_hal_crypto_gcm_decrypt_and_verify(
    const uint8_t* key,
    const uint8_t* iv,
    const uint8_t* aad,
    size_t aad_length,
    const uint8_t* input,
    uint8_t* output,
    size_t length,
    const uint8_t* tag) {
    uint8_t dtag[CRYPTO_GCM_TAG_LEN];

    if(!furi_hal_crypto_gcm(key, iv, aad, aad_length, input, output, length, dtag, true)) {
        memset(output, 0, length);
        return FuriHalCryptoGCMStateError;
    }

    if(!furi_hal_crypto_gcm_compare_tag(dtag, tag)) {
        memset(output, 0, length);
        return FuriHalCryptoGCMStateAuthFailure;
    }

    return FuriHalCryptoGCMStateOk;
}
