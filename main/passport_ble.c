#include "passport_ble.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_npl.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

/* ESP-IDF example uses this initializer without a public declaration. */
void ble_store_config_init(void);
static const char *TAG = "passport_ble";
static const char *NAME = "Passport Remote";
static QueueHandle_t s_commands;
static QueueHandle_t s_controls;
static bool s_enabled, s_pairing, s_clear_pending;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static remote_status_t s_status;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_report_handle;
static uint8_t s_addr_type;
static bool s_encrypted, s_subscribed, s_suspended, s_synced;
static bool s_need_zero;
static uint32_t s_next_advertise;
static media_pulse_t s_pulse;
static struct ble_npl_callout s_tick;
typedef struct { media_key_t key; uint32_t generation; } command_t;

/* One byte, five momentary consumer-control bits. GATT carries no Report ID;
 * the Report Reference descriptor tells Android that this is input report 1. */
static const uint8_t s_report_map[] = {
    0x05,0x0c, 0x09,0x01, 0xa1,0x01, 0x85,0x01,
    0x15,0x00, 0x25,0x01, 0x09,0xb6, 0x09,0xb5, 0x09,0xcd,
    0x09,0xe9, 0x09,0xea,
    0x75,0x01, 0x95,0x05, 0x81,0x02,
    0x75,0x03, 0x95,0x01, 0x81,0x03, 0xc0
};
enum { ATTR_INFO, ATTR_MAP, ATTR_REPORT, ATTR_REFERENCE, ATTR_CONTROL };

remote_status_t passport_ble_status(void)
{
    taskENTER_CRITICAL(&s_mux);
    remote_status_t status = s_status;
    taskEXIT_CRITICAL(&s_mux);
    return status;
}
static void set_state(remote_state_t state)
{
    taskENTER_CRITICAL(&s_mux);
    s_status.state = state;
    taskEXIT_CRITICAL(&s_mux);
}
static void count_result(bool success)
{
    taskENTER_CRITICAL(&s_mux);
    if (success) s_status.sent++; else s_status.failures++;
    taskEXIT_CRITICAL(&s_mux);
}
static void invalidate_commands(void)
{
    taskENTER_CRITICAL(&s_mux);
    s_status.generation++;
    taskEXIT_CRITICAL(&s_mux);
    xQueueReset(s_commands);
}
static bool ready(void)
{
    return s_enabled && !s_clear_pending && s_conn != BLE_HS_CONN_HANDLE_NONE && s_encrypted && s_subscribed && !s_suspended;
}
static void refresh_state(void)
{
    set_state(ready() ? REMOTE_READY : REMOTE_CONNECTED);
}
static int access_attr(uint16_t conn, uint16_t attr,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn; (void)attr;
    static const uint8_t info[] = {0x11,0x01,0x00,0x02};
    static const uint8_t reference[] = {1,1};
    const uint8_t *data = NULL;
    size_t length = 0;
    uintptr_t kind = (uintptr_t)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t value;
        if (OS_MBUF_PKTLEN(ctxt->om) != 1 || os_mbuf_copydata(ctxt->om, 0, 1, &value))
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if (kind == ATTR_CONTROL && value <= 1) {
            s_suspended = value == 0;
            invalidate_commands();
            s_need_zero = true;
            refresh_state();
            return 0;
        }
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    switch (kind) {
    case ATTR_INFO: data = info; length = sizeof(info); break;
    case ATTR_MAP: data = s_report_map; length = sizeof(s_report_map); break;
    case ATTR_REFERENCE: data = reference; length = sizeof(reference); break;
    case ATTR_REPORT: data = &s_pulse.held; length = 1; break;
    default: return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(ctxt->om, data, length) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
}

static const struct ble_gatt_svc_def s_services[] = {
    { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1812),
      .characteristics = (struct ble_gatt_chr_def[]) {
        {.uuid=BLE_UUID16_DECLARE(0x2a4a), .access_cb=access_attr,
         .arg=(void *)ATTR_INFO, .flags=BLE_GATT_CHR_F_READ},
        {.uuid=BLE_UUID16_DECLARE(0x2a4b), .access_cb=access_attr,
         .arg=(void *)ATTR_MAP, .flags=BLE_GATT_CHR_F_READ},
        {.uuid=BLE_UUID16_DECLARE(0x2a4d), .access_cb=access_attr,
         .arg=(void *)ATTR_REPORT, .val_handle=&s_report_handle,
         .flags=BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
         .descriptors=(struct ble_gatt_dsc_def[]) {
            {.uuid=BLE_UUID16_DECLARE(0x2908), .att_flags=BLE_ATT_F_READ,
             .access_cb=access_attr, .arg=(void *)ATTR_REFERENCE}, {0}}},
        {.uuid=BLE_UUID16_DECLARE(0x2a4c), .access_cb=access_attr,
         .arg=(void *)ATTR_CONTROL,
         .flags=BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC},
        {0}}}, {0}
};
static int gap_event(struct ble_gap_event *event, void *arg);
static int advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    static const ble_uuid16_t hid = BLE_UUID16_INIT(0x1812);
    fields.flags = BLE_HS_ADV_F_BREDR_UNSUP | (s_pairing ? BLE_HS_ADV_F_DISC_GEN : 0);
    fields.uuids16 = &hid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    fields.appearance = 0x03c0;
    fields.appearance_is_present = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc) return rc;
    memset(&fields, 0, sizeof(fields));
    fields.name = (const uint8_t *)NAME;
    fields.name_len = strlen(NAME);
    fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&fields);
    if (rc) return rc;
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = s_pairing ? BLE_GAP_DISC_MODE_GEN : BLE_GAP_DISC_MODE_NON;
    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (!rc) set_state(REMOTE_ADVERTISING);
    return rc;
}
static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status) { set_state(REMOTE_STARTING); break; }
        s_conn = event->connect.conn_handle;
        /* Normal mode accepts bonded identities only; new phones must use the
         * explicit pairing flow. Identity resolution is provided by NimBLE. */
        if (!s_enabled || !s_pairing) {
            struct ble_gap_conn_desc desc;
            ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
            int count=0; bool bonded=false;
            if (!ble_gap_conn_find(s_conn,&desc) &&
                !ble_store_util_bonded_peers(peers,&count,CONFIG_BT_NIMBLE_MAX_BONDS)) {
                for(int i=0;i<count;i++)
                    if(!memcmp(&peers[i],&desc.peer_id_addr,sizeof(ble_addr_t)))bonded=true;
            }
            if(!s_enabled || !bonded) {
                ble_gap_terminate(s_conn,BLE_ERR_REM_USER_CONN_TERM);
                break;
            }
        }
        s_encrypted = s_subscribed = s_suspended = false;
        s_need_zero = true;
        media_release_sent(&s_pulse);
        invalidate_commands();
        refresh_state();
        { int rc = ble_gap_security_initiate(s_conn);
          if (rc && rc != BLE_HS_EALREADY) ESP_LOGW(TAG, "Security start: %d", rc); }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        s_encrypted = s_subscribed = s_suspended = false;
        media_release_sent(&s_pulse);
        invalidate_commands();
        set_state(REMOTE_STARTING);
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc desc;
        s_encrypted = event->enc_change.status == 0 &&
            ble_gap_conn_find(s_conn, &desc) == 0 && desc.sec_state.encrypted;
        if (!s_encrypted) {
            invalidate_commands();
            ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        s_need_zero = true;
        if(s_encrypted) {
            s_pairing=false;
            taskENTER_CRITICAL(&s_mux);s_status.pairing=false;taskEXIT_CRITICAL(&s_mux);
        }
        refresh_state();
        break;
    }
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_report_handle) {
            s_subscribed = event->subscribe.cur_notify;
            invalidate_commands();
            s_need_zero = true;
            refresh_state();
        }
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        if(!s_pairing) return BLE_GAP_REPEAT_PAIRING_IGNORE;
        /* Android's "forget device" removes its key only. Replace just the
         * requesting peer's stale bond, preserving all other NVS contents. */
        { struct ble_gap_conn_desc desc;
          if (!ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) &&
              !ble_store_util_delete_peer(&desc.peer_id_addr))
              return BLE_GAP_REPEAT_PAIRING_RETRY; }
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    default: break;
    }
    return 0;
}
static int notify(uint8_t value)
{
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&value, 1);
    return om ? ble_gatts_notify_custom(s_conn, s_report_handle, om) : BLE_HS_ENOMEM;
}
static void tick(struct ble_npl_event *event)
{
    (void)event;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    passport_ble_op_t op;
    if (s_synced && xQueueReceive(s_controls,&op,0)==pdTRUE) {
        int rc=0;
        if(ble_gap_adv_active())rc=ble_gap_adv_stop();
        if(op==BLE_ENABLE)s_enabled=true;
        if(op==BLE_DISABLE){s_enabled=false;s_pairing=false;}
        if(op==BLE_PAIR && s_enabled)s_pairing=true;
        if(op==BLE_CLEAR){s_clear_pending=true;s_pairing=false;}
        invalidate_commands();media_release_sent(&s_pulse);s_need_zero=true;
        if(s_conn!=BLE_HS_CONN_HANDLE_NONE) {
            int e=ble_gap_terminate(s_conn,BLE_ERR_REM_USER_CONN_TERM);
            if(e)rc=e;
        }
        taskENTER_CRITICAL(&s_mux);
        s_status.pairing=s_pairing;s_status.controls++;s_status.control_error=rc;
        taskEXIT_CRITICAL(&s_mux);
        s_next_advertise=now+100;
    }
    if(s_clear_pending && s_conn==BLE_HS_CONN_HANDLE_NONE) {
        int rc=ble_store_clear();
        s_clear_pending=false;
        taskENTER_CRITICAL(&s_mux);
        s_status.controls++;s_status.control_error=rc;
        taskEXIT_CRITICAL(&s_mux);
    }
    if(!s_enabled && s_conn==BLE_HS_CONN_HANDLE_NONE)set_state(REMOTE_OFF);
    if (s_synced && s_enabled && !s_clear_pending && s_conn == BLE_HS_CONN_HANDLE_NONE && !ble_gap_adv_active() &&
        (int32_t)(now - s_next_advertise) >= 0) {
        int rc = advertise();
        s_next_advertise = now + 2000;
        if (rc) { set_state(REMOTE_FAILED); ESP_LOGW(TAG, "Advertise: %d", rc); }
    }
    if (ready()) {
        if (s_need_zero || media_release_due(&s_pulse, now)) {
            if (!notify(0)) { media_release_sent(&s_pulse); s_need_zero = false; }
            else count_result(false);
        } else if (!s_pulse.held) {
            command_t command;
            if (xQueueReceive(s_commands, &command, 0) == pdTRUE &&
                command.generation == passport_ble_status().generation) {
                int rc = notify((uint8_t)command.key);
                count_result(rc == 0);
                if (!rc) media_press_sent(&s_pulse, command.key, now);
            }
        }
    }
    ble_npl_callout_reset(&s_tick, ble_npl_time_ms_to_ticks32(20));
}
static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc) { set_state(REMOTE_FAILED); return; }
    s_synced = true;
    ble_npl_callout_reset(&s_tick, ble_npl_time_ms_to_ticks32(20));
}
static void on_reset(int reason)
{
    ESP_LOGE(TAG, "Host reset: %d", reason);
    s_synced = false;
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    s_encrypted = s_subscribed = s_suspended = false;
    media_release_sent(&s_pulse);
    invalidate_commands();
    set_state(REMOTE_FAILED);
}
static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    set_state(REMOTE_FAILED);
    vTaskDelete(NULL);
}
esp_err_t passport_ble_start(bool enabled)
{
    s_enabled=enabled;
    s_controls=xQueueCreate(8,sizeof(passport_ble_op_t));
    if(!s_controls){set_state(REMOTE_FAILED);return ESP_ERR_NO_MEM;}
    s_commands = xQueueCreate(8, sizeof(command_t));
    if (!s_commands) { vQueueDelete(s_controls);s_controls=NULL;set_state(REMOTE_FAILED); return ESP_ERR_NO_MEM; }
    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) { vQueueDelete(s_controls);s_controls=NULL;vQueueDelete(s_commands); s_commands = NULL; set_state(REMOTE_FAILED); return rc; }
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int err = ble_svc_gap_device_name_set(NAME);
    if (!err) err = ble_gatts_count_cfg(s_services);
    if (!err) err = ble_gatts_add_svcs(s_services);
    ble_store_config_init();
    ble_npl_callout_init(&s_tick, nimble_port_get_dflt_eventq(), tick, NULL);
    if (err || xTaskCreate(host_task, "passport_ble", 4096, NULL, 5, NULL) != pdPASS) {
        set_state(REMOTE_FAILED);
        ble_npl_callout_deinit(&s_tick);
        nimble_port_deinit();
        vQueueDelete(s_commands); s_commands = NULL;
        vQueueDelete(s_controls);s_controls=NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}
bool passport_ble_send(media_key_t key)
{
    if (key != MEDIA_PREVIOUS && key != MEDIA_NEXT && key != MEDIA_TOGGLE &&
        key != MEDIA_VOLUME_UP && key != MEDIA_VOLUME_DOWN) return false;
    remote_status_t status = passport_ble_status();
    if (status.state != REMOTE_READY || !s_commands) return false;
    command_t command = {.key = key, .generation = status.generation};
    return xQueueSend(s_commands, &command, 0) == pdTRUE;
}
bool passport_ble_control(passport_ble_op_t op)
{
    return s_controls && op<=BLE_CLEAR && xQueueSend(s_controls,&op,0)==pdTRUE;
}
