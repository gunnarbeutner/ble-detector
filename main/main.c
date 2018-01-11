/* BLE Detector + MQTT SN client */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>

#include "esp_log.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_event_loop.h"

#include "wifi_sta.h"

#include "MQTTSNPacket.h"
#include "transport.h"

#include "watchdog.h"
#include "report.h"
#include "btmod.h"
#include "hm10.h"

#include "user_config.h"

#define TAG "ble-detector"

static int mqtt_socket = -1;
static int mqtt_packetid;

static struct {
  const char *mac;
  const char *client_id;
  const char *wifi_ssid;
  int uart2_connected;
} device_settings[] = DEVICE_SETTINGS;

static const char *mqtt_client_id;
static const char *wifi_ssid;
static int uart2_connected;

static wifi_sta_init_struct_t wifi_params;

static void mqtt_publish(char *topicname, char *payload, size_t payload_len)
{
  unsigned char buf[512];
  int buflen = sizeof(buf);
  MQTTSN_topicid topic;

  topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
  topic.data.long_.name = topicname;
  topic.data.long_.len = strlen(topicname);

  int len = MQTTSNSerialize_publish(buf, buflen, 0, 3, 0, mqtt_packetid++,
      topic, (unsigned char *)payload, payload_len);
  transport_sendPacketBuffer(mqtt_socket, MQTT_HOST, MQTT_PORT, buf, len);
}

void publish_mac(char *mac, int rssi, int instance)
{
  if (mqtt_socket == -1)
    return;

  char topic[256];
  char payload[256];
  sprintf(topic, "happy-bubbles/ble/%s/raw/%s", mqtt_client_id, mac);
  int payload_len = sprintf(payload, "{\"hostname\":\"%s\",\"mac\":\"%s\",\"rssi\":%d,\"type\":\"0\",\"instance\":%d}", mqtt_client_id, mac, rssi, instance);
  printf("MQTT pub: %s\n", payload);

  mqtt_publish(topic, payload, payload_len);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch(event->event_id) {
  case SYSTEM_EVENT_STA_GOT_IP:
    if (mqtt_socket != -1) {
      transport_close(mqtt_socket);
      mqtt_socket = -1;
    }

    mqtt_packetid = 0;

    mqtt_socket = transport_open();

    if (mqtt_socket < 0) {
      printf("transport_open() failed");
      return ESP_OK;
    }

    //gpio_set_level(GPIO_NUM_2, 1);

    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    printf("WiFi got disassociated.\n");
    /* fallthrough */

  case SYSTEM_EVENT_STA_LOST_IP:
    printf("Lost IP address.\n");

    transport_close(mqtt_socket);
    mqtt_socket = -1;
    mqtt_packetid = 0;

    //gpio_set_level(GPIO_NUM_2, 0);

    break;
  default:
    break;
  }

  return ESP_OK;
}

static void init_wifi(void)
{
  printf("Setting up WIFI network connection.\n");

  wifi_params.network_ssid = wifi_ssid;
  wifi_params.network_password = WIFI_NETWORK_PASSWORD;

  wifi_sta_init(&wifi_params);
}

static esp_err_t app_event_handler(void *ctx, system_event_t *event)
{
  esp_err_t result = ESP_OK;
  int handled = 0;

  printf("app_event_handler: event: %d\n", event->event_id);

  // Let the wifi_sta module handle all WIFI STA events.

  result = wifi_sta_handle_event(ctx, event, &handled);
  event_handler(ctx, event);
  return result;
}


void app_main(void)
{
  printf("[APP] BLE SN, version: %d\n", SOFTWARE_VERSION);
  printf("[APP] Free memory: %d bytes\n", esp_get_free_heap_size());

  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_LOGW(TAG, "nvs_flash_init failed (0x%x), erasing partition and retrying", err);
    const esp_partition_t *nvs_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    assert(nvs_partition && "partition table must have an NVS partition");
    ESP_ERROR_CHECK(esp_partition_erase_range(nvs_partition, 0, nvs_partition->size));
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  esp_event_loop_init(&app_event_handler, NULL);

  unsigned char addr[6];
  esp_efuse_mac_get_default(addr);

  char mac[13];
  sprintf(mac, "%02x%02x%02x%02x%02x%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  int found = 0;

  for (int i = 0; i < sizeof(device_settings) / sizeof(device_settings[0]); i++) {
    if (strcmp(device_settings[i].mac, mac) == 0) {
      found = 1;

      mqtt_client_id = device_settings[i].client_id;
      wifi_ssid = device_settings[i].wifi_ssid;
      uart2_connected = device_settings[i].uart2_connected;

      break;
    }
  }

  if (!found) {
    printf("ERROR: No settings are available for this device (mac: %s)\n", mac);
    return;
  }

  printf("Device instance: %s\n", mqtt_client_id);

  init_wifi();

  tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, mqtt_client_id);

  init_beacon_reports();

  hm10_init_module_a();

  if (uart2_connected)
    hm10_init_module_b();

  init_btmod();

  for (;;) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    publish_beacon_reports();
  }
}
