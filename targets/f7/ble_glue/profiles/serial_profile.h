#pragma once

#include <furi_ble/profile_interface.h>

#include <services/serial_service.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_PROFILE_SERIAL_PACKET_SIZE_MAX BLE_SVC_SERIAL_DATA_LEN_MAX

typedef enum {
    FuriHalBtSerialRpcStatusNotActive,
    FuriHalBtSerialRpcStatusActive,
} FuriHalBtSerialRpcStatus;

/** Serial service callback type */
typedef SerialServiceEventCallback FuriHalBtSerialCallback;

/** Serial profile descriptor */
extern const FuriHalBleProfileTemplate* const ble_profile_serial;

/** Send data through BLE
 *
 * @param profile       Profile instance
 * @param data          data buffer
 * @param size          data buffer size
 *
 * @return      true on success
 */
bool ble_profile_serial_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);

/** Set BLE RPC status
 *
 * @param profile       Profile instance
 * @param active        true if RPC is active
 */
void ble_profile_serial_set_rpc_active(FuriHalBleProfileBase* profile, bool active);

/** Notify that application buffer is empty
 * @param profile       Profile instance
 */
void ble_profile_serial_notify_buffer_is_empty(FuriHalBleProfileBase* profile);

/** Set Serial service events callback
 *
 * @param profile       Profile instance
 * @param buffer_size   Applicaition buffer size
 * @param calback       FuriHalBtSerialCallback instance
 * @param context       pointer to context
 */
void ble_profile_serial_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSerialCallback callback,
    void* context);

/** Set custom data channel callback
 *
 * @param profile       Profile instance
 * @param callback      Callback for received custom data
 * @param context       Callback context
 */
void ble_profile_serial_set_custom_data_callback(
    FuriHalBleProfileBase* profile,
    SerialServiceCustomDataCallback callback,
    void* context);

/** Send custom data through BLE (uses fe66 characteristic)
 *
 * @param profile       Profile instance
 * @param data          data buffer
 * @param size          data buffer size
 *
 * @return      true on success
 */
bool ble_profile_serial_custom_data_tx(
    FuriHalBleProfileBase* profile,
    uint8_t* data,
    uint16_t size);

#ifdef __cplusplus
}
#endif
