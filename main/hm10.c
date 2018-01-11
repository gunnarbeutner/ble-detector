#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "watchdog.h"
#include "report.h"

#define TAG "hm10"

#define GSM_DEBUG 1

static watchdog watchdogs[2];

static void ble_scan(int uart_num)
{
  uart_write_bytes(uart_num, "AT+DISI?", 8);
  uart_wait_tx_done(uart_num, 100 / portTICK_RATE_MS);

  const int buf_size = 8192;
  char *buf = malloc(buf_size + 1);
  int read_offset = 0, write_offset = 0;

  for (;;) {
    int len = uart_read_bytes(uart_num, (uint8_t *)buf + write_offset, buf_size - 1 - write_offset, 10 / portTICK_RATE_MS);

    if (len == 0)
      continue;

    //ESP_LOGI("ble_scan", "uart_read_bytes(): %d for uart%d (buf: %d)", len, uart_num, write_offset - read_offset);

    write_offset += len;
    buf[write_offset] = '\0';

    //ESP_LOGW(TAG, "rbuf: %s", buf + read_offset);

    while (read_offset < write_offset) {
      char *rbuf = buf + read_offset;

      if (strstr(rbuf, "OK+DISIS") == rbuf) {
        //ESP_LOGI("ble_scan", "Discovery started for uart%d.", uart_num);
        read_offset += 8;

        continue;
      } else if (strstr(rbuf, "OK+DISC:") == rbuf) {
        //ESP_LOGI("ble_scan", "Discovery result for uart%d.", uart_num);

        if (write_offset - read_offset < 8 + 70)
          break;

        read_offset += 8;

        char mac[13], rssi[5];
        for (int i = 0; i < 12; i++)
          mac[i] = tolower((int)*(buf + read_offset + 53 + i));
        mac[12] = '\0';

        memcpy(rssi, buf + read_offset + 66, 4);
        rssi[4] = '\0';

        int rssi_num;
        rssi_num = rssi[3] - '0';
        rssi_num += 10 * (rssi[2] - '0');
        rssi_num += 100 * (rssi[1] - '0');

        if (rssi[0] == '-')
          rssi_num *= -1;

        report_beacon(mac, rssi_num, uart_num);

        read_offset += 70;

        continue;
      } else if (strstr(rbuf, "OK+DISCE") == rbuf) {
        ble_sn_watchdog_rearm(&watchdogs[uart_num - 1]);
        ESP_LOGI("ble_scan", "Discovery finished for uart%d.", uart_num);
        free(buf);
        return;
      }

      ESP_LOGE(TAG, "end of ble_scan inner loop, uart%d, left-over: %d", uart_num, write_offset - read_offset);
      break;
    }
  }
}

static int atCmd_waitResponse(int uart_num, char * cmd, char *resp, char * resp1, int cmdSize, int timeout, char **response, int size)
{
  char sresp[256] = {'\0'};
  char data[256] = {'\0'};
  int len, res = 1, idx = 0, tot = 0, timeoutCnt = 0;

  if (cmd != NULL) {
    if (cmdSize == -1) cmdSize = strlen(cmd);
#if GSM_DEBUG
    ESP_LOGI("uart", "AT COMMAND @ uart%d: %s", uart_num, cmd);
#endif
    uart_write_bytes(uart_num, (const char*)cmd, cmdSize);
    uart_wait_tx_done(uart_num, 100 / portTICK_RATE_MS);
  }

  if (response != NULL) {
    // Read GSM response into buffer
    char *pbuf = *response;
    len = uart_read_bytes(uart_num, (uint8_t *)data, sizeof(data), timeout / portTICK_RATE_MS);
    while (len > 0) {
      if ((tot+len) >= size) {
        char *ptemp = realloc(pbuf, size+512);
        if (ptemp == NULL) return 0;
        size += 512;
        pbuf = ptemp;
      }
      memcpy(pbuf+tot, data, len);
      tot += len;
      response[tot] = '\0';
      len = uart_read_bytes(uart_num, (uint8_t *)data, sizeof(data), 100 / portTICK_RATE_MS);
    }
    *response = pbuf;
    return tot;
  }

  // ** Wait for and check the response
  idx = 0;
  while(1)
  {
    memset(data, 0, 256);
    len = 0;
    len = uart_read_bytes(uart_num, (uint8_t *)data, sizeof(data), 10 / portTICK_RATE_MS);
    if (len > 0) {
      for (int i=0; i<len;i++) {
        if (idx < 256) {
          if ((data[i] >= 0x20) && (data[i] < 0x80)) sresp[idx++] = data[i];
          else sresp[idx++] = 0x2e;
        }
      }
      tot += len;
    }
    else {
      if (tot > 0) {
        // Check the response
        if (strstr(sresp, resp) != NULL) {
#if GSM_DEBUG
          ESP_LOGI("uart", "AT RESPONSE @ uart%d: [%s]", uart_num, sresp);
#endif
          break;
        }
        else {
          if (resp1 != NULL) {
            if (strstr(sresp, resp1) != NULL) {
#if GSM_DEBUG
              ESP_LOGI("uart", "AT RESPONSE (1) @ uart%d: [%s]", uart_num, sresp);
#endif
              res = 2;
              break;
            }
          }
          // no match
#if GSM_DEBUG
          ESP_LOGI("uart", "AT BAD RESPONSE @ uart%d: [%s]", uart_num, sresp);
#endif
          res = 0;
          break;
        }
      }
    }

    timeoutCnt += 10;
    if (timeoutCnt > timeout) {
      // timeout
#if GSM_DEBUG
      ESP_LOGE("uart", "AT: TIMEOUT @ uart%d", uart_num);
#endif
      res = 0;
      break;
    }
  }

  return res;
}

static void hm10_scan_task(void *uart_num_v)
{
  int uart_num = (int)uart_num_v;

/*  const int rates[] = { 230400, 115200, 57600, 38400, 19200, 9600, 4800, 2400 };

  printf("Detecting BLE modem baud rate...\n");

  for (int i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
    printf("Testing baudrate: %d\n", rates[i]);

    uart_set_baudrate(uart_num, rates[i]);

    for (int j = 0; j < 3; j++)
      if (atCmd_waitResponse(uart_num, "AT", "OK", NULL, 2, 1000, NULL, 0) != 0)
        goto detected_baudrate;
  }

  printf("Failed to detect baudrate.\n");
  return;

detected_baudrate:
  atCmd_waitResponse(uart_num, "AT+BAUD4", "OK", NULL, 8, 1000, NULL, 0);
  uart_set_baudrate(uart_num, 9600);*/

  while (atCmd_waitResponse(uart_num, "AT+RENEW", "OK", NULL, 8, 1000, NULL, 0) == 0) {
    vTaskDelay(1000 / portTICK_RATE_MS);
    uart_flush(uart_num);
		}

  while (atCmd_waitResponse(uart_num, "AT+PIO11", "OK", NULL, 8, 1000, NULL, 0) == 0)
    ;

  while (atCmd_waitResponse(uart_num, "AT+ROLE1", "OK", NULL, 8, 1000, NULL, 0) == 0)
    ;

  while (atCmd_waitResponse(uart_num, "AT+IMME1", "OK", NULL, 8, 1000, NULL, 0) == 0)
    ;

  while (atCmd_waitResponse(uart_num, "AT+RESET", "OK", NULL, 8, 1000, NULL, 0) == 0)
    ;

  vTaskDelay(1000 / portTICK_RATE_MS);

  while (atCmd_waitResponse(uart_num, "AT", "OK", NULL, 2, 1000, NULL, 0) == 0)
    ;

  while (1) {
    printf("Doing BLE discovery for uart%d...\n", uart_num);
    ble_scan(uart_num);
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

static uart_config_t uart_config1 = {
  .baud_rate = 9600,
  .data_bits = UART_DATA_8_BITS,
  .parity = UART_PARITY_DISABLE,
  .stop_bits = UART_STOP_BITS_1,
  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  //UART_HW_FLOWCTRL_CTS_RTS,
  .rx_flow_ctrl_thresh = 122,
};

void hm10_init_module_a()
{
  const int uart_num_a = UART_NUM_1;

  uart_param_config(uart_num_a, &uart_config1);
  uart_set_pin(uart_num_a, GPIO_NUM_5, GPIO_NUM_18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(uart_num_a, UART_FIFO_LEN * 2, 0, 0, NULL, 0);

  ble_init_watchdog(&watchdogs[0]);

  xTaskCreate(hm10_scan_task, "ble_scan_a", 8192, (void *)uart_num_a, 4, NULL);
}

void hm10_init_module_b()
{
  const int uart_num_b = UART_NUM_2;

  uart_param_config(uart_num_b, &uart_config1);
  uart_set_pin(uart_num_b, GPIO_NUM_19, GPIO_NUM_21, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(uart_num_b, UART_FIFO_LEN * 2, 0, 0, NULL, 0);

  ble_init_watchdog(&watchdogs[1]);

  xTaskCreate(hm10_scan_task, "ble_scan_b", 8192, (void *)uart_num_b, 4, NULL);
}
