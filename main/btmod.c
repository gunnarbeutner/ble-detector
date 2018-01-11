#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "watchdog.h"
#include "report.h"

static watchdog ble_watchdog;
static int ble_scan_state;
static int init_bt_completed = 0;

static esp_ble_scan_params_t ble_scan_params = {
  .scan_type = BLE_SCAN_TYPE_ACTIVE,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval = 0x50,
  .scan_window = 0x30,
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  switch (event) {
  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    ble_sn_watchdog_rearm(&ble_watchdog);

    printf("Starting Bluetooth scan.\n");
    ble_scan_state = 1;
    esp_ble_gap_start_scanning(5);

    break;
  }
  case ESP_GAP_BLE_SCAN_RESULT_EVT: {
    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
    switch (scan_result->scan_rst.search_evt) {
    case ESP_GAP_SEARCH_INQ_RES_EVT: {
      // forward beacon to MQTT
      uint8_t *addr = scan_result->scan_rst.bda;

      char mac[13];
      sprintf(mac, "%02x%02x%02x%02x%02x%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

      report_beacon(mac, scan_result->scan_rst.rssi, 0);

      break;
    }
    case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
      ble_sn_watchdog_rearm(&ble_watchdog);

      vTaskDelay(500 / portTICK_PERIOD_MS);

      printf("Resetting Bluetooth controller.\n");
      esp_bt_controller_disable();
      esp_bt_controller_enable(ESP_BT_MODE_BTDM);
      esp_ble_gap_set_scan_params(&ble_scan_params);

      break;
    }
    default:
      break;
    }
    break;
  }
  default:
    break;
  }
}

void init_btmod()
{
  printf("Initialising Bluetooth controller.\n");

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

  if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
    printf("ERROR: esp_bt_controller_init() failed.\n");
    return;
  }

  if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
    printf("ERROR: esp_bt_controller_enable() failed.\n");
    return;
  }

  if (esp_bluedroid_init() != ESP_OK) {
    printf("ERROR: esp_bluedroid_init() failed.\n");
    return;
  }

  if (esp_bluedroid_enable() != ESP_OK) {
    printf("ERROR: esp_bluedroid_enable() failed.\n");
    return;
  }

  if (esp_ble_gap_register_callback(esp_gap_cb) != ESP_OK) {
    printf("ERROR: esp_ble_gap_register_callback() failed.\n");
    return;
  }

  ESP_ERROR_CHECK(ble_init_watchdog(&ble_watchdog));

  init_bt_completed = 1;

  printf("Starting Bluetooth scan.\n");

  if (esp_ble_gap_set_scan_params(&ble_scan_params) != ESP_OK) {
    printf("ERROR: esp_ble_gap_set_scan_params() failed.\n");
    return;
  }
}

void btmod_start_scan()
{
  ble_scan_state = 1;

  if (esp_ble_gap_start_scanning(5) != ESP_OK) {
    printf("ERROR: esp_ble_gap_start_scanning() failed.\n");
    ble_scan_state = 0;
  }
}

void btmod_stop_scan()
{
  printf("Stopping Bluetooth scan.\n");

  if (esp_ble_gap_stop_scanning() != ESP_OK)
    printf("ERROR: esp_ble_gap_stop_scanning() failed.\n");

  ble_scan_state = 0;
}
