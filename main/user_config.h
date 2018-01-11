/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __USER_CONFIG_LOCAL_H__
#define __USER_CONFIG_LOCAL_H__

#define SOFTWARE_VERSION 8

#define WIFI_NETWORK_PASSWORD "xxx"

#define MQTT_HOST "192.168.3.2"
#define MQTT_PORT 1883

#define DEVICE_SETTINGS { \
		{ "30aea40b6638", "hb-livingroom", "Kingdom of Frost", 1 }, \
		{ "30aea40b0954", "hb-kitchen", "Kingdom of Frost", 0 }, \
		{ "240ac482a8b8", "hb-bedroom", "Kingdom of Frost", 0 }, \
		{ "30aea405fc74", "hb-bathroom", "KoFDist", 0 }, \
		{ "30aea40d8520", "x-test", "Kingdom of Frost", 0 } \
	}

#endif
