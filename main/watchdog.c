#include "watchdog.h"

static void ble_sn_watchdog_timeout(TimerHandle_t xTimer)
{
  printf("BLE SN watchdog timeout.\n");
  esp_restart();
}

esp_err_t ble_init_watchdog(watchdog *wd)
{
  wd->timer = xTimerCreate("BLE SN Watchdog", 20000 / portTICK_PERIOD_MS, pdFALSE, NULL, &ble_sn_watchdog_timeout);

  if (!wd->timer) {
    printf("ERROR: xTimerCreate() failed.\n");
    return ESP_FAIL;
  }

  if (xTimerStart(wd->timer, 0) != pdPASS) {
    printf("ERROR: xTimerStart() failed.\n");
    return ESP_FAIL;
  }

  return ESP_OK;
}

void ble_sn_watchdog_rearm(watchdog *wd)
{
  xTimerChangePeriod(wd->timer, 15000 / portTICK_PERIOD_MS, 0);
}
