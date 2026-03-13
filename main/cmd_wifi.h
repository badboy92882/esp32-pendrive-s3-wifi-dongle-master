/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CMD_WIFI_H
#define __CMD_WIFI_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi_types.h"  // <--- for wifi_promiscuous_pkt_type_t

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Wi-Fi in sniffer mode
void initialise_wifi_sniffer(void);
void deinit_wifi_sniffer(void);

// Station & AP commands
esp_err_t wifi_cmd_sta_join(const char* ssid, const char* pass);
esp_err_t wifi_cmd_ap_set(const char* ssid, const char* pass);
esp_err_t wifi_cmd_sta_scan(const char* ssid);
esp_err_t wifi_cmd_query(void);
esp_err_t wifi_cmd_set_mode(char* mode);
esp_err_t wifi_cmd_start_smart_config(void);
esp_err_t wifi_cmd_stop_smart_config(void);
esp_err_t wif_cmd_disconnect_wifi(void);

// Wi-Fi buffer management
void wifi_buffer_free(void *buffer, void *ctx);

// Wi-Fi promiscuous callback
void wifi_recv_callback(void *buf, wifi_promiscuous_pkt_type_t type);

// Get local IP
uint32_t wifi_get_local_ip(void);

#ifdef __cplusplus
}
#endif

#endif // __CMD_WIFI_H