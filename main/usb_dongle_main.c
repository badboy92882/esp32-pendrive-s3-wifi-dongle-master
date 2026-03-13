/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified: Replace WiFi STA mode with SoftAP + lwIP NAPT so that
 *   Mac (USB-ECM / Internet Sharing) --> ESP32-S3 --> iPhone (WiFi AP)
 *
 * SSID    : mm
 * Password: 1122334455
 *
 * Internet flows:  Mac --[USB-ECM 192.168.2.x]--> ESP --[NAT]--> AP 192.168.4.x --> iPhone
 * iTunes sync  :  add static route on Mac once after boot:
 *                   sudo route add 192.168.4.0/24 <ESP_ECM_IP>
 */
 
#include <stdio.h>
#include <string.h>
 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
 
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
 
#include "nvs_flash.h"
 
/* lwIP NAPT — requires CONFIG_LWIP_IP_FORWARD=y and CONFIG_LWIP_IPV4_NAPT=y in sdkconfig */
#include "lwip/lwip_napt.h"
 
/* USB / TinyUSB — only tinyusb.h is a public header in esp_tinyusb 2.x / IDF 5.x
 * tusb_net.h and tusb_config.h are internal — do NOT include them.
 * ECM network is auto-initialized inside tinyusb_driver_install()
 * when CONFIG_TINYUSB_NET_MODE_ECM=y is set in sdkconfig. */
#include "tinyusb.h"
 
/* CLI commands intentionally removed — cmd_system.h and cmd_wifi.h are not
 * needed for the USB ECM + SoftAP + NAPT functionality */
 
/* ------------------------------------------------------------------ */
 
#define TAG  "USB_Dongle"
 
/* SoftAP config — change here if needed */
#define AP_SSID          "mm"
#define AP_PASSWORD      "1122334455"
#define AP_CHANNEL       6
#define AP_MAX_CONN      4
#define AP_BEACON_INT    100   /* ms */
 
/* ------------------------------------------------------------------ */
/*  WiFi event handler                                                  */
/* ------------------------------------------------------------------ */
 
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
 
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "SoftAP started — SSID: %s", AP_SSID);
            break;
 
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station connected, AID=%d", e->aid);
            break;
        }
 
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station disconnected, AID=%d", e->aid);
            break;
        }
 
        default:
            break;
        }
    }
}
 
/* ------------------------------------------------------------------ */
/*  SoftAP + NAPT initialisation                                        */
/*                                                                      */
/*  The ESP has two network interfaces after this call:                 */
/*    • ECM/RNDIS  — created by tusb_net (upstream, gets IP from Mac)  */
/*    • WIFI_AP    — 192.168.4.1/24  (downstream, DHCP for iPhone)     */
/*                                                                      */
/*  NAPT is enabled on the AP interface so all packets from 192.168.4.x*/
/*  are masqueraded and forwarded out through the ECM interface.        */
/* ------------------------------------------------------------------ */
 
static void wifi_init_softap(void)
{
    /* Create the default AP netif — DHCP server and 192.168.4.1 GW built-in */
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif != NULL);
 
    /* Initialise the WiFi driver with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
 
    /* Register event handler for AP events */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
 
    /* Build AP config */
    wifi_config_t wifi_config = {
        .ap = {
            .ssid           = AP_SSID,
            .ssid_len       = (uint8_t)strlen(AP_SSID),
            .channel        = AP_CHANNEL,
            .password       = AP_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .beacon_interval = AP_BEACON_INT,
            .authmode       = WIFI_AUTH_WPA2_PSK,
        },
    };
 
    /* If no password is set fall back to open AP */
    if (strlen(AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
 
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
 
    ESP_LOGI(TAG, "SoftAP init done — SSID:%s  PW:%s  CH:%d",
             AP_SSID, AP_PASSWORD, AP_CHANNEL);
 
    /*
     * Enable NAPT on the AP interface.
     *
     * This makes the ESP act as a NAT router:
     *   iPhone (192.168.4.x)  --->  ESP NAPT  --->  Mac ECM (192.168.2.x)  --->  Internet
     *
     * The AP netif default GW IP is 192.168.4.1.  We pass that address to
     * ip_napt_enable() so lwIP knows which interface to masquerade.
     *
     * Note: the ECM interface must be up and have received an IP from macOS
     * Internet Sharing before traffic actually flows — but NAPT can be
     * enabled before that without issue.
     */
    esp_netif_ip_info_t ap_ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ap_ip_info));
    ip_napt_enable(ap_ip_info.ip.addr, 1);
 
    ESP_LOGI(TAG, "NAPT enabled on AP interface (" IPSTR ")",
             IP2STR(&ap_ip_info.ip));
}
 
/* ------------------------------------------------------------------ */
/*  app_main                                                            */
/* ------------------------------------------------------------------ */
 
void app_main(void)
{
    /* ---- NVS (required by WiFi) ----------------------------------- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
 
    /* ---- Core networking stack ------------------------------------ */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
 
    /* ---- USB ECM gadget init (unchanged from original project) ---- */
    /*
     * tusb_net registers the USB network interface with lwIP.
     * macOS Internet Sharing assigns an IP to this interface
     * (typically 192.168.2.2) and acts as the default gateway (192.168.2.1).
     * All upstream NAPT traffic egresses here.
     */
    ESP_LOGI(TAG, "USB initialization");
 
    /*
     * Zero-init is explicitly recommended by Espressif for IDF v4.4:
     *   tinyusb_config_t tusb_cfg = { 0 }
     * Any field left 0/NULL uses the menuconfig default value.
     * This avoids field-name mismatches between IDF versions
     * (.descriptor in v4.4 vs .device_descriptor in later IDF).
     */
    /* tinyusb_config_t comes from managed_components/espressif__esp_tinyusb/include/tinyusb.h
     * (local components/tinyusb_dongle/include/tinyusb.h must be renamed to avoid shadowing it)
     * task_stack_size MUST be non-zero or driver install will return ESP_ERR_INVALID_ARG */
    tinyusb_config_t tusb_cfg = {
        .task = { .size = 4096, .priority = 5, .xCoreID = 0 },
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    /* ECM network interface is auto-initialized by tinyusb_driver_install()
     * when CONFIG_TINYUSB_NET_MODE_ECM=y — no separate tinyusb_net_init() needed */
 
    ESP_LOGI(TAG, "USB initialization done");
 
    /* CLI commands removed — not needed for this project */
 
    /* ---- Start SoftAP + enable NAPT ------------------------------ */
    wifi_init_softap();
 
    /*
     * Nothing else to do in app_main — the FreeRTOS scheduler keeps
     * running the USB task, lwIP task, and WiFi task indefinitely.
     *
     * Once:
     *   1. USB cable is plugged into Mac
     *   2. macOS Internet Sharing is enabled for the USB Ethernet adapter
     *   3. iPhone connects to WiFi SSID "mm" with password "1122334455"
     *
     * …the iPhone will have full internet access via the Mac's connection.
     *
     * For iTunes WiFi sync visibility, run once on the Mac:
     *   sudo route add 192.168.4.0/24 <ESP_ECM_IP>
     *   (ESP_ECM_IP is shown in the Mac Network settings, usually 192.168.2.2)
     */
    ESP_LOGI(TAG, "Ready. Waiting for iPhone to connect to SSID: %s", AP_SSID);
}
