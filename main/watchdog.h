#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

typedef struct {
  TimerHandle_t timer;
} watchdog;

esp_err_t ble_init_watchdog(watchdog *wd);
void ble_sn_watchdog_rearm(watchdog *wd);
