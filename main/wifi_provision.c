#include "wifi_provision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

static const char *TAG = "wifi_provision";
static bool s_prepared;
static bool s_wifi_started;
static bool s_manager_initialized;
static bool s_provisioning;
static bool s_connected;
static wifi_provision_state_t s_state = WIFI_PROVISION_IDLE;
static char s_service_name[20] = "FoloToy-??????";

static void set_state(wifi_provision_state_t state)
{
    s_state = state;
    ESP_LOGI(TAG, "state=%s", wifi_provision_state_text());
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_PROV_EVENT) {
        switch (id) {
            case WIFI_PROV_START:
                s_provisioning = true;
                set_state(WIFI_PROVISION_WAITING);
                break;
            case WIFI_PROV_CRED_RECV:
                set_state(WIFI_PROVISION_CONNECTING);
                break;
            case WIFI_PROV_CRED_SUCCESS:
                s_provisioning = false;
                set_state(WIFI_PROVISION_CONNECTING);
                break;
            case WIFI_PROV_CRED_FAIL:
                s_provisioning = true;
                set_state(WIFI_PROVISION_FAILED);
                break;
            case WIFI_PROV_END:
                s_provisioning = false;
                if (s_manager_initialized) {
                    wifi_prov_mgr_deinit();
                    s_manager_initialized = false;
                }
                break;
            default:
                break;
        }
    } else if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            if (!s_provisioning) set_state(WIFI_PROVISION_FAILED);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        set_state(WIFI_PROVISION_CONNECTED);
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_setservername(1, "pool.ntp.org");
        setenv("TZ", "CST-8", 1);
        tzset();
        esp_sntp_init();
        ESP_LOGI(TAG, "Wi-Fi connected; NTP synchronization requested");
    }
}

static esp_err_t manager_init(void)
{
    if (s_manager_initialized) return ESP_OK;
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    esp_err_t err = wifi_prov_mgr_init(config);
    if (err == ESP_OK) s_manager_initialized = true;
    return err;
}

static void start_station(void)
{
    if (s_wifi_started) return;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;
}

void wifi_provision_prepare(void)
{
    if (s_prepared) return;
    s_prepared = true;

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_service_name, sizeof(s_service_name), "FoloToy-%02X%02X%02X", mac[3], mac[4], mac[5]);

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
        esp_netif_create_default_wifi_sta();
    }
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
}

void wifi_provision_auto_start(void)
{
    wifi_provision_prepare();
    wifi_config_t config = {0};
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        set_state(WIFI_PROVISION_FAILED);
        return;
    }
    err = esp_wifi_get_config(WIFI_IF_STA, &config);
    if (err != ESP_OK) {
        set_state(WIFI_PROVISION_FAILED);
        return;
    }
    if (config.sta.ssid[0] != '\0') {
        set_state(WIFI_PROVISION_CONNECTING);
        start_station();
    }
}

void wifi_provision_start(void)
{
    wifi_provision_prepare();
    if (s_provisioning) return;
    set_state(WIFI_PROVISION_STARTING);
    esp_err_t err = manager_init();
    if (err != ESP_OK) {
        set_state(WIFI_PROVISION_FAILED);
        return;
    }
    /* Security 0 keeps this first test build self-contained: the phone app
     * still asks the user for the home SSID/password and the device stores
     * them in the ESP-IDF Wi-Fi NVS partition. No home credentials are here. */
    err = wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_0, NULL, s_service_name, NULL);
    if (err != ESP_OK) {
        wifi_prov_mgr_deinit();
        s_manager_initialized = false;
        set_state(WIFI_PROVISION_FAILED);
    }
}

void wifi_provision_retry(void)
{
    wifi_provision_stop();
    wifi_provision_start();
}

void wifi_provision_stop(void)
{
    if (s_manager_initialized) {
        wifi_prov_mgr_stop_provisioning();
        wifi_prov_mgr_deinit();
        s_manager_initialized = false;
    }
    s_provisioning = false;
    if (!s_connected) set_state(WIFI_PROVISION_IDLE);
}

bool wifi_provision_is_connected(void)
{
    return s_connected;
}

wifi_provision_state_t wifi_provision_state(void)
{
    return s_state;
}

const char *wifi_provision_service_name(void)
{
    return s_service_name;
}

const char *wifi_provision_state_text(void)
{
    switch (s_state) {
        case WIFI_PROVISION_STARTING: return "启动中";
        case WIFI_PROVISION_WAITING: return "等待手机连接";
        case WIFI_PROVISION_CONNECTING: return "连接中";
        case WIFI_PROVISION_CONNECTED: return "已连接";
        case WIFI_PROVISION_FAILED: return "连接失败 按上重试";
        default: return "未配网";
    }
}
