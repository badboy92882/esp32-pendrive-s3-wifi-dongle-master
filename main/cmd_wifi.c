#include <string.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"  // <--- for wifi_promiscuous_pkt_type_t
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"

#include "cmd_wifi.h"

static const char *TAG = "CMD_WIFI";

/**
 * Example Wi-Fi sniffer callback
 */
void wifi_recv_callback(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA && type != WIFI_PKT_CTRL) {
        return;
    }

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    ESP_LOGI(TAG, "Sniffer packet length: %d", pkt->rx_ctrl.sig_len);
}

/**
 * Initialize Wi-Fi in promiscuous mode
 */
void initialise_wifi_sniffer(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_recv_callback));

    ESP_LOGI(TAG, "Wi-Fi sniffer initialized");
}

/**
 * Stop Wi-Fi promiscuous mode
 */
void deinit_wifi_sniffer(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_LOGI(TAG, "Wi-Fi sniffer stopped");
}

/**
 * Get local IP in station mode
 */
uint32_t wifi_get_local_ip(void)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *sta_if = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_if) return 0;

    if (esp_netif_get_ip_info(sta_if, &ip_info) == ESP_OK) {
        return ip_info.ip.addr;  // network byte order
    }
    return 0;
}

// ===== Dummy placeholders =====
esp_err_t wifi_cmd_sta_join(const char* ssid, const char* pass) { return ESP_OK; }
esp_err_t wifi_cmd_ap_set(const char* ssid, const char* pass) { return ESP_OK; }
esp_err_t wifi_cmd_sta_scan(const char* ssid) { return ESP_OK; }
esp_err_t wifi_cmd_query(void) { return ESP_OK; }
esp_err_t wifi_cmd_set_mode(char* mode) { return ESP_OK; }
esp_err_t wifi_cmd_start_smart_config(void) { return ESP_OK; }
esp_err_t wifi_cmd_stop_smart_config(void) { return ESP_OK; }
esp_err_t wif_cmd_disconnect_wifi(void) { return ESP_OK; }
void wifi_buffer_free(void *buffer, void *ctx) {}