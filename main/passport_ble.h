#pragma once
#include "esp_err.h"
#include "passport_model.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { REMOTE_STARTING, REMOTE_ADVERTISING, REMOTE_CONNECTED,
               REMOTE_READY, REMOTE_FAILED, REMOTE_OFF } remote_state_t;
typedef enum { BLE_ENABLE, BLE_DISABLE, BLE_PAIR, BLE_CLEAR } passport_ble_op_t;
typedef struct {
    remote_state_t state;
    uint32_t generation;
    uint32_t sent;
    uint32_t failures;
    uint32_t controls;
    int control_error;
    bool pairing;
} remote_status_t;
esp_err_t passport_ble_start(bool enabled);
bool passport_ble_control(passport_ble_op_t operation);
remote_status_t passport_ble_status(void);
/* Nonblocking enqueue. True means queued, not acknowledged by the player. */
bool passport_ble_send(media_key_t key);
