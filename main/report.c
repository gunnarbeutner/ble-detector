#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "report.h"
#include "main.h"

typedef struct {
  char mac[13];
  int rssi;
  int instance;
} beacon_report;

static beacon_report beacon_reports[512];
static int beacon_reports_index;
static SemaphoreHandle_t beacon_reports_mutex;
static int led_state;

void report_beacon(char *mac, int rssi, int instance)
{
  xSemaphoreTake(beacon_reports_mutex, portMAX_DELAY);

  beacon_report *report = &beacon_reports[beacon_reports_index];
  memcpy(report->mac, mac, 13);
  report->rssi = rssi;
  report->instance = instance;
  beacon_reports_index++;

  xSemaphoreGive(beacon_reports_mutex);
}

static int compare_beacon(const void *a, const void *b)
{
  const beacon_report *ra = (const beacon_report *)a;
  const beacon_report *rb = (const beacon_report *)b;

  int rc = strcmp(ra->mac, rb->mac);

  if (rc != 0)
    return rc;

  if (ra->instance < rb->instance)
    return -1;
  else if (ra->instance > rb->instance)
    return 1;
  else
    return 0;
}

void publish_beacon_reports()
{
  gpio_set_level(GPIO_NUM_2, (led_state++) % 2);

  xSemaphoreTake(beacon_reports_mutex, portMAX_DELAY);

  qsort(beacon_reports, beacon_reports_index, sizeof(beacon_report), compare_beacon);

  double rssi_total = 0;
  int rssi_count = 0;

  for (int i = 0; i < beacon_reports_index; i++) {
    beacon_report *rep = &beacon_reports[i];

    rssi_total += pow(10, (double)rep->rssi / 10.0);
    rssi_count++;

    int last_source = 0;
    beacon_report *next_rep = &beacon_reports[i + 1];

    if (i == beacon_reports_index - 1)
      last_source = 1;
    else if (memcmp(rep->mac, next_rep->mac, 13) != 0)
      last_source = 1;
    else if (rep->instance != next_rep->instance)
      last_source = 1;

    if (last_source) {
      double rssi_avg = 10 * log10(rssi_total / rssi_count);

      //printf("{%s, %d}: %f (*%d) vs. %d\n", rep->mac, rep->instance, rssi_avg, rssi_count, rep->rssi);
      publish_mac(rep->mac, rssi_avg, rep->instance);

      rssi_total = 0;
      rssi_count = 0;
    }
  }

  beacon_reports_index = 0;

  xSemaphoreGive(beacon_reports_mutex);
}

void init_beacon_reports()
{
  beacon_reports_mutex = xSemaphoreCreateMutex();
}
