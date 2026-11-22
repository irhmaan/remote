/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_hid_gap.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"

#define GATT_SVR_SVC_HID_UUID 0x1812

static const char *TAG = "ESP_HID_GAP";
static struct ble_hs_adv_fields s_adv_fields;
static ble_uuid16_t s_hid_uuid = BLE_UUID16_INIT(GATT_SVR_SVC_HID_UUID);

extern void ble_hid_task_start_up(void);

esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name)
{
    if (device_name == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_adv_fields, 0, sizeof(s_adv_fields));
    s_adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    s_adv_fields.appearance = appearance;
    s_adv_fields.appearance_is_present = 1;
    s_adv_fields.tx_pwr_lvl_is_present = 1;
    s_adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    s_adv_fields.name = (uint8_t *)device_name;
    s_adv_fields.name_len = strlen(device_name);
    s_adv_fields.name_is_complete = 1;
    s_adv_fields.uuids16 = &s_hid_uuid;
    s_adv_fields.num_uuids16 = 1;
    s_adv_fields.uuids16_is_complete = 1;

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;

    return ESP_OK;
}

static int nimble_hid_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        break;
    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "connection updated; status=%d", event->conn_update.status);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG,
                 "subscribe event; conn_handle=%d attr_handle=%d reason=%d "
                 "prev_notify=%d cur_notify=%d prev_indicate=%d cur_indicate=%d",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.reason,
                 event->subscribe.prev_notify,
                 event->subscribe.cur_notify,
                 event->subscribe.prev_indicate,
                 event->subscribe.cur_indicate);
        break;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "encryption change event; status=%d", event->enc_change.status);
        if (event->enc_change.status == 0)
        {
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            if (rc == 0)
            {
                ble_hid_task_start_up();
            }
        }
        else
        {
            ESP_LOGW(TAG, "encryption failed; waiting for disconnect/retry");
        }
        break;
    case BLE_GAP_EVENT_NOTIFY_TX:
        ESP_LOGI(TAG,
                 "notify_tx event; conn_handle=%d attr_handle=%d status=%d is_indication=%d",
                 event->notify_tx.conn_handle,
                 event->notify_tx.attr_handle,
                 event->notify_tx.status,
                 event->notify_tx.indication);
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0)
        {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
    {
        ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started");
        struct ble_sm_io pkey = {0};

        if (event->passkey.params.action == BLE_SM_IOACT_DISP)
        {
            pkey.action = BLE_SM_IOACT_DISP;
            pkey.passkey = 123456;
            ESP_LOGI(TAG, "Enter passkey %" PRIu32 " on the peer side", pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP)
        {
            ESP_LOGI(TAG, "Accepting passkey");
            pkey.action = BLE_SM_IOACT_NUMCMP;
            pkey.numcmp_accept = 1;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_OOB)
        {
            pkey.action = BLE_SM_IOACT_OOB;
            memset(pkey.oob, 0, sizeof(pkey.oob));
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_INPUT)
        {
            ESP_LOGI(TAG, "Input not supported; passing 123456");
            pkey.action = BLE_SM_IOACT_INPUT;
            pkey.passkey = 123456;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

esp_err_t esp_hid_ble_gap_adv_start(void)
{
    int rc = ble_gap_adv_set_fields(&s_adv_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return ESP_FAIL;
    }

    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = BLE_GAP_ADV_ITVL_MS(30),
        .itvl_max = BLE_GAP_ADV_ITVL_MS(50),
    };
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, 180000,
                           &adv_params, nimble_hid_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t init_low_level(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_nimble_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_nimble_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static esp_err_t deinit_low_level(void)
{
    esp_err_t ret = esp_nimble_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_nimble_deinit failed: %s", esp_err_to_name(ret));
    }

    esp_err_t controller_ret = esp_bt_controller_disable();
    if (controller_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_controller_disable failed: %s", esp_err_to_name(controller_ret));
        if (ret == ESP_OK)
        {
            ret = controller_ret;
        }
    }

    controller_ret = esp_bt_controller_deinit();
    if (controller_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_controller_deinit failed: %s", esp_err_to_name(controller_ret));
        if (ret == ESP_OK)
        {
            ret = controller_ret;
        }
    }
    return ret;
}

esp_err_t esp_hid_gap_deinit(void)
{
    return deinit_low_level();
}

esp_err_t esp_hid_gap_init(uint8_t mode)
{
    if (mode != HIDD_BLE_MODE)
    {
        ESP_LOGE(TAG, "Only BLE/NimBLE mode is supported");
        return ESP_ERR_INVALID_ARG;
    }
    return init_low_level();
}
