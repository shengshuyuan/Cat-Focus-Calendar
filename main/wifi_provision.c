#include "wifi_provision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include "clock_time.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "lwip/ip_addr.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_provision";

#define SCAN_MAX 20
#define FORM_MAX 192
#define HTML_MAX 6144

typedef struct {
    char ssid[33];
    int8_t rssi;
} scan_item_t;

static bool s_prepared;
static bool s_wifi_started;
static bool s_ap_up;
static bool s_connected;
static bool s_trying_sta;
static bool s_httpd_started;
static wifi_provision_state_t s_state = WIFI_PROVISION_IDLE;
static char s_service_name[20] = "FoloToy-??????";
static httpd_handle_t s_httpd;
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;

static scan_item_t s_scan_items[SCAN_MAX];
static int s_scan_count;
static volatile bool s_scan_busy;

/* Ignore first STA_DISCONNECTED after disconnect()+connect(). */
static volatile int s_connect_ignore_disc;
static volatile int s_connect_fail_streak;

static char s_pending_ssid[33];
static char s_pending_pass[65];
static bool s_pending_cred;

static void set_state(wifi_provision_state_t state)
{
    s_state = state;
    ESP_LOGI(TAG, "state=%s heap=%lu", wifi_provision_state_text(),
             (unsigned long)esp_get_free_heap_size());
}

static void sntp_sync_cb(struct timeval *tv)
{
    (void)tv;
    clock_time_on_sntp_sync();
}

static void persist_sta_creds(void)
{
    if (!s_pending_cred || !s_pending_ssid[0]) return;
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, s_pending_ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, s_pending_pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = s_pending_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    ESP_LOGI(TAG, "persist STA ssid=%s err=%s", s_pending_ssid, esp_err_to_name(err));
}


static esp_err_t http_date_header_cb(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;
    if (!evt->header_key || !evt->header_value) return ESP_OK;
    if (strcasecmp(evt->header_key, "Date") != 0) return ESP_OK;

    /* Date: Fri, 11 Sep 2026 07:20:00 GMT */
    struct tm tm_utc = {0};
    if (!strptime(evt->header_value, "%a, %d %b %Y %H:%M:%S GMT", &tm_utc)) {
        ESP_LOGW(TAG, "bad Date header: %s", evt->header_value);
        return ESP_OK;
    }
    char *old_tz = getenv("TZ");
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t sec = mktime(&tm_utc);
    if (old_tz) setenv("TZ", old_tz, 1);
    else setenv("TZ", "CST-8", 1);
    tzset();
    if (sec < 1704067200LL) return ESP_OK;
    struct timeval tv = {.tv_sec = sec, .tv_usec = 0};
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "HTTP Date sync ok: %s", evt->header_value);
    clock_time_on_sntp_sync();
    return ESP_OK;
}

static bool http_time_sync_once_ex(const char *url, bool use_get)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = use_get ? HTTP_METHOD_GET : HTTP_METHOD_HEAD,
        .timeout_ms = 4000,
        .event_handler = http_date_header_cb,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "HTTP time try %s (%s) -> %s status=%d", url, use_get ? "GET" : "HEAD",
             esp_err_to_name(err), status);
    return !clock_time_needs_sync_hint();
}

static bool http_time_sync_once(const char *url)
{
    return http_time_sync_once_ex(url, false);
}

static bool http_time_sync_gateway(void)
{
    if (!s_sta_netif) s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!s_sta_netif) return false;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(s_sta_netif, &ip) != ESP_OK || ip.gw.addr == 0) return false;
    char url[64];
    snprintf(url, sizeof(url), "http://" IPSTR "/", IP2STR(&ip.gw));
    ESP_LOGI(TAG, "try gateway Date %s", url);
    if (http_time_sync_once_ex(url, true)) return true;
    return http_time_sync_once_ex(url, false);
}


static void force_public_dns(void)
{
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    if (!s_sta_netif) return;
    esp_netif_dns_info_t dns = {0};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr("223.5.5.5"); /* AliDNS */
    esp_err_t err = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
    ESP_LOGI(TAG, "set DNS 223.5.5.5 -> %s", esp_err_to_name(err));
    dns.ip.u_addr.ip4.addr = ipaddr_addr("119.29.29.29"); /* DNSPod */
    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
}

static void ntp_watch_task(void *arg)
{
    (void)arg;
    for (int i = 0; i < 90; i++) {
        if (!clock_time_needs_sync_hint()) {
            ESP_LOGI(TAG, "time already synced");
            vTaskDelete(NULL);
            return;
        }
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            ESP_LOGI(TAG, "SNTP status completed after %ds", i);
            clock_time_on_sntp_sync();
            vTaskDelete(NULL);
            return;
        }
        /* After 12s without SNTP, fall back to HTTP Date (UDP/123 often blocked). */
        if (i == 12 || i == 25 || i == 40) {
            ESP_LOGW(TAG, "SNTP slow — trying HTTP Date fallback");
            force_public_dns();
            if (http_time_sync_gateway() ||
                http_time_sync_once("http://www.baidu.com") ||
                http_time_sync_once("http://ntp.aliyun.com") ||
                http_time_sync_once("http://connect.rom.miui.com/generate_204") ||
                http_time_sync_once("http://www.msftconnecttest.com/connecttest.txt")) {
                vTaskDelete(NULL);
                return;
            }
        }
        if ((i % 10) == 9) {
            ESP_LOGW(TAG, "SNTP still waiting (%ds) status=%d", i + 1,
                     (int)esp_sntp_get_sync_status());
            esp_sntp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGE(TAG, "time sync timeout");
    vTaskDelete(NULL);
}

static void start_ntp(void)
{
    force_public_dns();
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    /* Prefer IP first in case DNS is flaky on first connect. */
    ip_addr_t aliyun;
    if (ipaddr_aton("203.107.6.88", &aliyun)) {
        esp_sntp_setserver(0, &aliyun);
    } else {
        esp_sntp_setservername(0, "ntp.aliyun.com");
    }
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_setservername(2, "cn.pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);
    if (esp_sntp_enabled()) {
        esp_sntp_restart();
        ESP_LOGI(TAG, "SNTP restarted");
    } else {
        esp_sntp_init();
        ESP_LOGI(TAG, "SNTP init");
    }
    static bool watch_started;
    if (!watch_started) {
        watch_started = true;
        if (xTaskCreate(ntp_watch_task, "ntp_watch", 3072, NULL, 5, NULL) != pdPASS) {
            watch_started = false;
            ESP_LOGW(TAG, "ntp_watch task failed");
        }
    }
}

static void stop_httpd(void)
{
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        s_httpd_started = false;
    }
}

static void stop_softap(void)
{
    stop_httpd();
    if (s_ap_up) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        s_ap_up = false;
    }
}

static void delayed_stop_softap_task(void *arg)
{
    (void)arg;
    /* Keep SoftAP briefly so phone can see success, then drop AP and NTP on STA. */
    vTaskDelay(pdMS_TO_TICKS(3500));
    persist_sta_creds();
    stop_softap();
    vTaskDelay(pdMS_TO_TICKS(500));
    start_ntp();
    vTaskDelete(NULL);
}

static void schedule_stop_softap(void)
{
    if (xTaskCreate(delayed_stop_softap_task, "ap_stop", 2048, NULL, 5, NULL) != pdPASS) {
        persist_sta_creds();
        stop_softap();
        start_ntp();
    }
}


static int url_decode(char *dst, size_t dst_len, const char *src, size_t src_len)
{
    size_t o = 0;
    for (size_t i = 0; i < src_len && o + 1 < dst_len; i++) {
        char c = src[i];
        if (c == '+') {
            dst[o++] = ' ';
        } else if (c == '%' && i + 2 < src_len) {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            dst[o++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = 0;
    return (int)o;
}

static bool form_get(const char *body, const char *key, char *out, size_t out_len)
{
    size_t key_len = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            const char *end = strchr(p, '&');
            size_t n = end ? (size_t)(end - p) : strlen(p);
            url_decode(out, out_len, p, n);
            return out[0] != 0;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    out[0] = 0;
    return false;
}

static void html_escape_ssid(char *dst, size_t dst_len, const char *src)
{
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 6 < dst_len; i++) {
        char c = src[i];
        if (c == '&') o += (size_t)snprintf(dst + o, dst_len - o, "&amp;");
        else if (c == '<') o += (size_t)snprintf(dst + o, dst_len - o, "&lt;");
        else if (c == '>') o += (size_t)snprintf(dst + o, dst_len - o, "&gt;");
        else if (c == '"') o += (size_t)snprintf(dst + o, dst_len - o, "&quot;");
        else dst[o++] = c;
    }
    dst[o] = 0;
}

static void do_scan(void)
{
    s_scan_count = 0;
    memset(s_scan_items, 0, sizeof(s_scan_items));
    esp_wifi_scan_stop();

    wifi_scan_config_t cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 220,
    };
    esp_err_t err = esp_wifi_scan_start(&cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan_start failed: %s", esp_err_to_name(err));
        return;
    }

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num == 0) {
        ESP_LOGW(TAG, "scan returned 0 APs");
        return;
    }
    uint16_t n = ap_num > SCAN_MAX ? SCAN_MAX : ap_num;
    wifi_ap_record_t rec[SCAN_MAX];
    memset(rec, 0, sizeof(rec));
    if (esp_wifi_scan_get_ap_records(&n, rec) != ESP_OK) {
        ESP_LOGW(TAG, "get_ap_records failed");
        return;
    }

    for (uint16_t i = 0; i < n; i++) {
        if (rec[i].ssid[0] == 0) continue;
        bool dup = false;
        for (int j = 0; j < s_scan_count; j++) {
            if (strncmp(s_scan_items[j].ssid, (const char *)rec[i].ssid, 32) == 0) {
                if (rec[i].rssi > s_scan_items[j].rssi) {
                    s_scan_items[j].rssi = rec[i].rssi;
                }
                dup = true;
                break;
            }
        }
        if (dup || s_scan_count >= SCAN_MAX) continue;
        memcpy(s_scan_items[s_scan_count].ssid, rec[i].ssid, 32);
        s_scan_items[s_scan_count].ssid[32] = 0;
        s_scan_items[s_scan_count].rssi = rec[i].rssi;
        s_scan_count++;
    }

    for (int i = 0; i < s_scan_count; i++) {
        for (int j = i + 1; j < s_scan_count; j++) {
            if (s_scan_items[j].rssi > s_scan_items[i].rssi) {
                scan_item_t tmp = s_scan_items[i];
                s_scan_items[i] = s_scan_items[j];
                s_scan_items[j] = tmp;
            }
        }
    }
    ESP_LOGI(TAG, "scan count=%d raw=%u heap=%lu", s_scan_count, (unsigned)ap_num,
             (unsigned long)esp_get_free_heap_size());
}

static void scan_task(void *arg)
{
    (void)arg;
    do_scan();
    s_scan_busy = false;
    vTaskDelete(NULL);
}

static bool request_scan_async(void)
{
    if (s_scan_busy) return false;
    s_scan_busy = true;
    if (xTaskCreate(scan_task, "wifi_scan", 4096, NULL, 5, NULL) != pdPASS) {
        s_scan_busy = false;
        ESP_LOGW(TAG, "scan task create failed");
        return false;
    }
    return true;
}

static esp_err_t send_html(httpd_req_t *req, const char *extra)
{
    char *page = malloc(HTML_MAX);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
        return ESP_FAIL;
    }
    size_t used = 0;
    used += (size_t)snprintf(page + used, HTML_MAX - used,
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1,maximum-scale=1\">"
        "<title>配网</title><style>"
        "body{margin:0;background:#f5f0e3;color:#17263a;font-family:-apple-system,sans-serif}"
        ".wrap{max-width:420px;margin:0 auto;padding:16px}"
        ".card{background:#fff;border-radius:14px;padding:16px;box-shadow:0 1px 6px rgba(0,0,0,.08)}"
        "h1{font-size:18px;margin:0 0 4px}"
        ".sub{color:#7c7a70;font-size:13px;margin:0 0 14px;line-height:1.4}"
        ".row{display:flex;align-items:center;justify-content:space-between;gap:8px;margin:0 0 8px}"
        ".row h2{font-size:15px;margin:0}"
        ".link{color:#304b38;font-size:14px;text-decoration:none;white-space:nowrap;border:0;background:0;padding:0;font:inherit}"
        ".list{border:1px solid #e6decc;border-radius:10px;max-height:220px;overflow:auto;margin:0 0 12px}"
        ".list.hide{display:none}"
        ".item{display:flex;gap:10px;align-items:center;padding:10px 12px;border-bottom:1px solid #f0e9db}"
        ".item:last-child{border-bottom:0}.meta{flex:1;min-width:0}"
        ".ssid{font-size:15px;line-height:1.3;word-break:break-all}"
        ".sig{color:#7c7a70;font-size:12px;margin-top:2px}"
        ".waves{display:flex;align-items:flex-end;gap:2px;height:16px;flex-shrink:0}"
        ".waves b{display:block;width:3px;background:#d2c9b5;border-radius:1px}"
        ".waves b:nth-child(1){height:4px}.waves b:nth-child(2){height:8px}"
        ".waves b:nth-child(3){height:12px}.waves b:nth-child(4){height:16px}"
        ".waves.l4 b,.waves.l3 b:nth-child(-n+3),.waves.l2 b:nth-child(-n+2),"
        ".waves.l1 b:nth-child(1){background:#304b38}"
        ".picked{display:none;align-items:center;justify-content:space-between;gap:8px;"
        "border:1px solid #e6decc;border-radius:10px;padding:12px;margin:0 0 12px;background:#faf7f0}"
        ".picked.on{display:flex}.picked .n{font-size:15px;font-weight:600;word-break:break-all}"
        "label.field{display:block;font-size:13px;color:#506a4d;margin:10px 0 4px}"
        "input[type=text],input[type=password]{width:100%%;box-sizing:border-box;font-size:16px;"
        "padding:11px 12px;border:1px solid #d8cdb6;border-radius:10px;background:#fff}"
        "details{margin:10px 0 0}summary{color:#7c7a70;font-size:13px}"
        ".btn{display:block;width:100%%;margin-top:14px;padding:13px;border:0;border-radius:12px;"
        "background:#304b38;color:#fff;font-size:16px}"
        ".tip{color:#7c7a70;font-size:12px;line-height:1.45;margin:12px 0 0}"
        ".msg{background:#eef5ea;color:#304b38;border-radius:8px;padding:8px 10px;font-size:13px;margin:0 0 12px}"
        ".scan{text-align:center;padding:18px 8px;color:#506a4d}"
        "</style></head><body><div class=wrap><div class=card>"
        "<h1>猫猫专注日历</h1>"
        "<p class=sub>浏览器访问 <b>192.168.4.1</b><br>仅支持 2.4GHz Wi-Fi</p>");

    if (extra && extra[0]) {
        used += (size_t)snprintf(page + used, HTML_MAX - used,
                                 "<div class=msg>%s</div>", extra);
    }

    if (s_scan_busy) {
        used += (size_t)snprintf(page + used, HTML_MAX - used,
            "<div class=scan>正在扫描附近网络…</div>"
            "<a class=link href=/ style=\"display:block;text-align:center;padding:12px\">查看结果</a>"
            "<meta http-equiv=refresh content=\"2;url=/\">");
    } else {
        used += (size_t)snprintf(page + used, HTML_MAX - used,
            "<form method=POST action=/save id=f>"
            "<div class=row>"
            "<h2>选择网络 · %d</h2>"
            "<a class=link href=/rescan>刷新</a>"
            "</div>", s_scan_count);

        used += (size_t)snprintf(page + used, HTML_MAX - used,
            "<div id=picked class=picked>"
            "<span class=n id=pickedName></span>"
            "<button type=button class=link id=change>更换</button>"
            "</div>");

        if (s_scan_count <= 0) {
            used += (size_t)snprintf(page + used, HTML_MAX - used,
                "<div id=list class=list><div class=item>"
                "<div class=meta><div class=ssid>暂无可用网络</div>"
                "<div class=sig>点右上角刷新，或下方手动输入</div></div></div></div>");
        } else {
            used += (size_t)snprintf(page + used, HTML_MAX - used, "<div id=list class=list>");
            for (int i = 0; i < s_scan_count && used + 200 < HTML_MAX; i++) {
                char esc[96];
                html_escape_ssid(esc, sizeof(esc), s_scan_items[i].ssid);
                int lvl = s_scan_items[i].rssi > -55 ? 4 :
                          s_scan_items[i].rssi > -70 ? 3 :
                          s_scan_items[i].rssi > -80 ? 2 : 1;
                used += (size_t)snprintf(page + used, HTML_MAX - used,
                    "<label class=item>"
                    "<input type=radio name=s value=\"%s\">"
                    "<div class=meta><div class=ssid>%s</div></div>"
                    "<span class=\"waves l%d\" aria-hidden=true>"
                    "<b></b><b></b><b></b><b></b></span></label>",
                    esc, esc, lvl);
            }
            used += (size_t)snprintf(page + used, HTML_MAX - used, "</div>");
        }

        used += (size_t)snprintf(page + used, HTML_MAX - used,
            "<label class=field for=pw>密码</label>"
            "<input id=pw name=p type=password maxlength=63 "
            "placeholder=\"选网后在此输入，无密码可留空\" autocomplete=current-password "
            "enterkeyhint=done>"
            "<details id=man><summary>列表没有？手动输入名称</summary>"
            "<label class=field>Wi-Fi 名称</label>"
            "<input name=t type=text maxlength=32 placeholder=\"2.4G 名称\" id=manSsid>"
            "</details>"
            "<button class=btn type=submit>连接</button>"
            "</form>"
            "<p class=tip>先点选网络，列表会收起后再输密码。刷新可能短暂掉线，连回热点即可。最多约 20 个。</p>"
            "<script>"
            "(function(){"
            "var L=document.getElementById('list'),P=document.getElementById('picked'),"
            "N=document.getElementById('pickedName'),C=document.getElementById('change'),"
            "W=document.getElementById('pw'),M=document.getElementById('man');"
            "function pick(el){"
            "if(!el)return;"
            "N.textContent=el.value;"
            "P.classList.add('on');"
            "if(L)L.classList.add('hide');"
            "if(M)M.open=false;"
            "setTimeout(function(){W&&W.focus()},50);"
            "}"
            "function showList(){"
            "P.classList.remove('on');"
            "if(L)L.classList.remove('hide');"
            "var r=document.querySelector('input[name=s]:checked');"
            "if(r)r.checked=false;"
            "}"
            "document.querySelectorAll('input[name=s]').forEach(function(r){"
            "r.addEventListener('change',function(){pick(r)});"
            "});"
            "if(C)C.addEventListener('click',showList);"
            "if(M)M.addEventListener('toggle',function(){"
            "if(M.open){showList();var t=document.getElementById('manSsid');t&&t.focus();}"
            "});"
            "})();"
            "</script>");
    }

    used += (size_t)snprintf(page + used, HTML_MAX - used, "</div></div></body></html>");
    if (used >= HTML_MAX - 1) {
        ESP_LOGW(TAG, "portal html truncated used=%u", (unsigned)used);
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    esp_err_t err = httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    free(page);
    return err;
}

static esp_err_t root_get(httpd_req_t *req)
{
    return send_html(req, NULL);
}

static esp_err_t rescan_get(httpd_req_t *req)
{
    if (s_scan_busy) {
        return send_html(req, NULL);
    }
    if (!request_scan_async()) {
        return send_html(req, "暂时无法刷新，请稍后再试");
    }
    return send_html(req, NULL);
}

static esp_err_t save_post(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > FORM_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "form too large");
        return ESP_FAIL;
    }
    char body[FORM_MAX + 1];
    int got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv fail");
            return ESP_FAIL;
        }
        got += r;
    }
    body[got] = 0;

    char ssid[33] = {0};
    char typed[33] = {0};
    char pass[64] = {0};
    form_get(body, "s", ssid, sizeof(ssid));
    form_get(body, "t", typed, sizeof(typed));
    form_get(body, "p", pass, sizeof(pass));
    if (typed[0]) strncpy(ssid, typed, sizeof(ssid) - 1);
    if (!ssid[0]) return send_html(req, "请选择或输入 Wi-Fi 名称");

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    memset(s_pending_ssid, 0, sizeof(s_pending_ssid));
    memset(s_pending_pass, 0, sizeof(s_pending_pass));
    strncpy(s_pending_ssid, ssid, sizeof(s_pending_ssid) - 1);
    strncpy(s_pending_pass, pass, sizeof(s_pending_pass) - 1);
    s_pending_cred = true;

    /* Write Flash immediately so reboot keeps the network even if later steps race. */
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    s_trying_sta = true;
    s_connect_ignore_disc = 1;
    s_connect_fail_streak = 0;
    set_state(WIFI_PROVISION_CONNECTING);
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        s_trying_sta = false;
        set_state(WIFI_PROVISION_FAILED);
        return send_html(req, "启动连接失败，请重试");
    }

    const char *wait =
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>连接中</title><style>"
        "body{margin:0;background:#f5f0e3;color:#17263a;font-family:-apple-system,sans-serif}"
        ".w{max-width:420px;margin:0 auto;padding:16px}"
        ".c{background:#fff;border-radius:14px;padding:20px;box-shadow:0 1px 6px rgba(0,0,0,.08)}"
        "h1{font-size:18px;margin:0 0 10px}.m{font-size:15px;line-height:1.5;margin:0 0 12px}"
        ".ok{color:#304b38}.bad{color:#a33}.tip{color:#7c7a70;font-size:13px;line-height:1.45}"
        "a{color:#304b38}</style></head><body><div class=w><div class=c>"
        "<h1>猫猫专注日历</h1>"
        "<p class=m id=msg>正在连接 Wi-Fi…</p>"
        "<p class=tip id=tip>请保持手机连着设备热点。成功后热点会自动关闭。</p>"
        "<p class=tip id=back style=\"display:none\"><a href=/>返回重试</a></p>"
        "</div></div><script>"
        "(function(){var n=0;function tick(){fetch('/status').then(function(r){return r.json()})."
        "then(function(j){var m=document.getElementById('msg'),t=document.getElementById('tip'),"
        "b=document.getElementById('back');"
        "if(j.s==='ok'){m.className='m ok';m.textContent='已连接成功';"
        "t.textContent='可以离开本页。设备热点即将关闭，时间会自动校准。';return;}"
        "if(j.s==='fail'){m.className='m bad';m.textContent='连接失败';"
        "t.textContent='请确认 2.4G 名称和密码后重试。';b.style.display='block';return;}"
        "n++;if(n>45){m.className='m bad';m.textContent='仍在等待，可返回重试';"
        "b.style.display='block';return;}setTimeout(tick,1000);})."
        "catch(function(){setTimeout(tick,1500)});}tick();})();"
        "</script></body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, wait, HTTPD_RESP_USE_STRLEN);
}


static esp_err_t status_get(httpd_req_t *req)
{
    const char *s = "wait";
    if (s_state == WIFI_PROVISION_CONNECTED || s_connected) s = "ok";
    else if (s_state == WIFI_PROVISION_FAILED) s = "fail";
    else if (s_state == WIFI_PROVISION_CONNECTING || s_trying_sta) s = "connecting";
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"s\":\"%s\"}", s);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t captive_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t start_httpd(void)
{
    if (s_httpd_started) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 3;
    config.lru_purge_enable = true;
    config.stack_size = 6144;
    config.recv_wait_timeout = 4;
    config.send_wait_timeout = 4;
    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) return err;

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    const httpd_uri_t rescan = {.uri = "/rescan", .method = HTTP_GET, .handler = rescan_get};
    const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
    const httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = status_get};
    const httpd_uri_t g204 = {.uri = "/generate_204", .method = HTTP_GET, .handler = captive_get};
    const httpd_uri_t hotspot = {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_get};
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &rescan);
    httpd_register_uri_handler(s_httpd, &save);
    httpd_register_uri_handler(s_httpd, &status);
    httpd_register_uri_handler(s_httpd, &g204);
    httpd_register_uri_handler(s_httpd, &hotspot);
    s_httpd_started = true;
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            if (!s_ap_up && s_state == WIFI_PROVISION_CONNECTING) esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            if (s_connect_ignore_disc > 0) {
                s_connect_ignore_disc--;
            } else if (s_trying_sta) {
                s_connect_fail_streak++;
                if (s_connect_fail_streak < 2) {
                    esp_wifi_connect();
                } else {
                    s_trying_sta = false;
                    set_state(WIFI_PROVISION_FAILED);
                }
            } else if (!s_ap_up) {
                set_state(WIFI_PROVISION_FAILED);
            }
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        s_trying_sta = false;
        s_connect_fail_streak = 0;
        set_state(WIFI_PROVISION_CONNECTED);
        persist_sta_creds();
        ESP_LOGI(TAG, "GOT_IP — scheduling SoftAP stop + NTP");
        if (s_ap_up) {
            schedule_stop_softap();
        } else {
            start_ntp();
        }
    }
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
        s_sta_netif = esp_netif_create_default_wifi_sta();
    } else {
        s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
}

static void ensure_wifi_started(void)
{
    if (s_wifi_started) return;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;
}

void wifi_provision_auto_start(void)
{
    wifi_provision_prepare();
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    wifi_config_t config = {0};
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) {
        set_state(WIFI_PROVISION_FAILED);
        return;
    }
    if (config.sta.ssid[0] != '\0') {
        ESP_LOGI(TAG, "auto-connect ssid=%s", (const char *)config.sta.ssid);
        /* Keep a copy so GOT_IP can re-persist if needed. */
        strncpy(s_pending_ssid, (const char *)config.sta.ssid, sizeof(s_pending_ssid) - 1);
        strncpy(s_pending_pass, (const char *)config.sta.password, sizeof(s_pending_pass) - 1);
        s_pending_cred = true;
        s_trying_sta = true;
        s_connect_ignore_disc = 0;
        s_connect_fail_streak = 0;
        set_state(WIFI_PROVISION_CONNECTING);
        ensure_wifi_started();
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG, "no saved STA ssid");
    }
}

void wifi_provision_start(void)
{
    wifi_provision_prepare();
    if (s_ap_up) return;

    set_state(WIFI_PROVISION_STARTING);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    do_scan();

    if (!s_ap_netif) {
        if (!esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
            s_ap_netif = esp_netif_create_default_wifi_ap();
        } else {
            s_ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        }
    }

    wifi_config_t ap = {0};
    strncpy((char *)ap.ap.ssid, s_service_name, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(s_service_name);
    ap.ap.max_connection = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.channel = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    s_ap_up = true;

    if (start_httpd() != ESP_OK) {
        set_state(WIFI_PROVISION_FAILED);
        stop_softap();
        return;
    }
    set_state(WIFI_PROVISION_WAITING);
}

void wifi_provision_retry(void)
{
    wifi_provision_stop();
    wifi_provision_start();
}

void wifi_provision_stop(void)
{
    s_trying_sta = false;
    stop_softap();
    if (!s_connected) set_state(WIFI_PROVISION_IDLE);
}

bool wifi_provision_is_connected(void) { return s_connected; }
wifi_provision_state_t wifi_provision_state(void) { return s_state; }
const char *wifi_provision_service_name(void) { return s_service_name; }

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
