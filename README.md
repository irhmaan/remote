| Supported Targets - ESP32-S3 |

# Esp32S2 Based Ble Remote

A remote ble device to control music playback.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name> # esp32s3 in our case
```


### Hardware Required

* A development board with ESP32 SoC (e.g., ESP32-S3)
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output.

```
idf.py build
idf.py -p PORT flash monitor 
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

##  NimBLE Logging Scheme

```
I (198) cpu_start: Multicore app
I (207) cpu_start: GPIO 44 and 43 are used as console UART I/O pins
I (302) main_task: Started on CPU0
I (302) main_task: Calling app_main()
I (312) ESP32-Remote: GPIO configuration done
I (312) ESP32-Remote: setting hid gap, mode:1
I (312) BLE_INIT: BT controller compile version [51d9dfd]
I (322) BLE_INIT: Using main XTAL as clock source
I (322) BLE_INIT: Feature Config, ADV:1, BLE_50:1, DTM:1, SCAN:1, CCA:0, SMP:1, CONNECT:1
I (382) ESP32-Remote: setting ble device
I (382) ESP32-Remote: BLE Host Task Started
I (392) NimBLE: GAP procedure initiated: stop advertising.

I (392) main_task: Returned from app_main()
I (392) HID_DEV_BLE: START
I (392) NimBLE: GAP procedure initiated: advertise; 
I (402) NimBLE: disc_mode=2
I (402) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=48 adv_itvl_max=80
I (74592) ESP_HID_GAP: connection established; status=0
I (74592) HID_DEV_BLE: CONNECT
I (74972) ESP_HID_GAP: PASSKEY_ACTION_EVENT started
I (74972) ESP_HID_GAP: Enter passkey 123456 on the peer side
I (74972) ESP_HID_GAP: ble_sm_inject_io result: 0
I (81162) NimBLE: GAP procedure initiated: stop advertising.

I (81172) ESP_HID_GAP: encryption change event; status=0
I (81552) ESP_HID_GAP: connection updated; status=0
I (81692) ESP_HID_GAP: subscribe event; conn_handle=1 attr_handle=8 reason=1 prev_notify=0 cur_notify=0 prev_indicate=0 cur_indicate=1
I (82102) ESP_HID_GAP: connection updated; status=0
I (82912) ESP_HID_GAP: subscribe event; conn_handle=1 attr_handle=37 reason=1 prev_notify=0 cur_notify=1 prev_indicate=0 cur_indicate=0
...
```



## Troubleshooting
