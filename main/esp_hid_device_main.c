/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#endif

#include "esp_hidd.h"
#include "esp_hid_gap.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static const char *TAG = "ESP32-Remote";
#define HID_BATTERY_LEVEL 50

#define BTN_GPIO 0
#define KEY_PLAY_PAUSE 5
#define KEY_NEXT 10
#define KEY_PREV 11

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
} local_param_t;

#if CONFIG_BT_NIMBLE_ENABLED
static local_param_t s_ble_hid_param = {0};

const unsigned char mediaReportMap[] = {
    0x05,
    0x0C, // Usage Page (Consumer)
    0x09,
    0x01, // Usage (Consumer Control)
    0xA1,
    0x01, // Collection (Application)
    0x85,
    0x03, // Report ID (3)
    0x09,
    0x02, // Usage (Numeric Key Pad)
    0xA1,
    0x02, // Collection (Logical)
    0x05,
    0x09, // Usage Page (Button)
    0x19,
    0x01, // Usage Minimum (0x01)
    0x29,
    0x0A, // Usage Maximum (0x0A)
    0x15,
    0x01, // Logical Minimum (1)
    0x25,
    0x0A, // Logical Maximum (10)
    0x75,
    0x04, // Report Size (4)
    0x95,
    0x01, // Report Count (1)
    0x81,
    0x00, // Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, //   End Collection
    0x05,
    0x0C, // Usage Page (Consumer)
    0x09,
    0x86, // Usage (Channel)
    0x15,
    0xFF, // Logical Minimum (-1)
    0x25,
    0x01, // Logical Maximum (1)
    0x75,
    0x02, // Report Size (2)
    0x95,
    0x01, // Report Count (1)
    0x81,
    0x46, // Input (Data,Var,Rel,No Wrap,Linear,Preferred State,Null State)
    0x09,
    0xE9, // Usage (Volume Increment)
    0x09,
    0xEA, // Usage (Volume Decrement)
    0x15,
    0x00, // Logical Minimum (0)
    0x75,
    0x01, // Report Size (1)
    0x95,
    0x02, // Report Count (2)
    0x81,
    0x02, // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09,
    0xE2, // Usage (Mute)
    0x09,
    0x30, // Usage (Power)
    0x09,
    0x83, // Usage (Recall Last)
    0x09,
    0x81, // Usage (Assign Selection)
    0x09,
    0xCD, // Usage (Play/Pause)
    0x09,
    0xB1, // Usage (Pause)
    0x09,
    0xB2, // Usage (Record)
    0x09,
    0xB3, // Usage (Fast Forward)
    0x09,
    0xB4, // Usage (Rewind)
    0x09,
    0xB5, // Usage (Scan Next Track)
    0x09,
    0xB6, // Usage (Scan Previous Track)
    0x09,
    0xB7, // Usage (Stop)
    0x15,
    0x01, // Logical Minimum (1)
    0x25,
    0x0C, // Logical Maximum (12)
    0x75,
    0x04, // Report Size (4)
    0x95,
    0x01, // Report Count (1)
    0x81,
    0x00, // Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09,
    0x80, // Usage (Selection)
    0xA1,
    0x02, // Collection (Logical)
    0x05,
    0x09, // Usage Page (Button)
    0x19,
    0x01, // Usage Minimum (0x01)
    0x29,
    0x03, // Usage Maximum (0x03)
    0x15,
    0x01, // Logical Minimum (1)
    0x25,
    0x03, // Logical Maximum (3)
    0x75,
    0x02, // Report Size (2)
    0x81,
    0x00, // Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, //   End Collection
    0x81,
    0x03, // Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, // End Collection
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {.data = mediaReportMap,
     .len = sizeof(mediaReportMap)}};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "ESP32 Remote",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = ble_report_maps,
    .report_maps_len = 1};

/* Report ID 3, 2 bytes: byte0 = keypad/channel/vol up (bit6)/vol down (bit7),
   byte1 low nibble = media key. */
static void send_media_key(uint8_t key)
{
    uint8_t buf[2] = {0, key};
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, 3, buf, 2); // press
    vTaskDelay(pdMS_TO_TICKS(30));
    buf[1] = 0;
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, 3, buf, 2); // release
}

static esp_err_t button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "GPIO configuration done");
    return ESP_OK;
}

// new function
void ble_hid_demo_task(void *pvParameters)
{

    int clicks = 0, prev = 1;
    int64_t last = 0;

    while (1)
    {
        int now = gpio_get_level(BTN_GPIO);
        int64_t t = esp_timer_get_time() / 1000;

        if (prev == 1 && now == 0 && t - last > 40)
        {
            clicks++;
            last = t;
        }
        if (clicks && t - last > 350)
        {
            ESP_LOGI(TAG, "clicks = %d", clicks);
            if (clicks == 1)
                send_media_key(KEY_PLAY_PAUSE);
            else if (clicks == 2)
                send_media_key(KEY_NEXT);
            else
                send_media_key(KEY_PREV);
            clicks = 0;
        }
        prev = now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif

void ble_hid_task_start_up(void)
{
    if (s_ble_hid_param.task_hdl)
    {
        // Task already exists
        return;
    }
    xTaskCreate(ble_hid_demo_task, "ble_hid_demo_task", 3 * 1024, NULL, configMAX_PRIORITIES - 3,
                &s_ble_hid_param.task_hdl);
}

void ble_hid_task_shut_down(void)
{
    if (s_ble_hid_param.task_hdl)
    {
        vTaskDelete(s_ble_hid_param.task_hdl);
        s_ble_hid_param.task_hdl = NULL;
    }
}

#if CONFIG_BT_NIMBLE_ENABLED
static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = "HID_DEV_BLE";

    switch (event)
    {
    case ESP_HIDD_START_EVENT:
    {
        ESP_LOGI(TAG, "START");
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT:
    {
        ESP_LOGI(TAG, "CONNECT");
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT:
    {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT:
    {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        if (param->control.control)
        {
            // exit suspend
            ble_hid_task_start_up();
        }
        else
        {
            // suspend
            ble_hid_task_shut_down();
        }
        break;
    }
    case ESP_HIDD_OUTPUT_EVENT:
    {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        break;
    }
    case ESP_HIDD_FEATURE_EVENT:
    {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT:
    {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        ble_hid_task_shut_down();
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_STOP_EVENT:
    {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}
#endif

#if CONFIG_BT_NIMBLE_ENABLED
void ble_hid_device_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}
void ble_store_config_init(void);
#endif

void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (button_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Button init failed, continuing without button");
    }

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_DEV_MODE);
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK(ret);

#if CONFIG_BT_NIMBLE_ENABLED
    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, ble_hid_config.device_name);

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));
    ESP_ERROR_CHECK(esp_hidd_dev_battery_set(s_ble_hid_param.hid_dev, HID_BATTERY_LEVEL));
#endif

#if CONFIG_BT_NIMBLE_ENABLED
    /* XXX Need to have template for store */
    ble_store_config_init();

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    /* Starting nimble task after gatts is initialized*/
    nimble_port_freertos_init(ble_hid_device_host_task);
#endif
}
