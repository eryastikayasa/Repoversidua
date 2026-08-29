#include "web_config.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "WEB_CONFIG";

#define CONFIG_NAMESPACE "config"
#define KEY_WIFI_SSID    "wifi_ssid"
#define KEY_WIFI_PASS    "wifi_pass"
#define KEY_API_KEY      "api_key"
#define KEY_ROLE_TEXT    "role_text"
#define KEY_FORCE_CONFIG "force_config"

// Halaman HTML sederhana
static const char *HTML_FORM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Config</title>
<style>
  body { font-family: sans-serif; background: #1e1e24; color: #f0f0f5; padding: 20px; }
  input, textarea { width: 100%; padding: 10px; margin: 6px 0; border-radius: 6px; border: 1px solid #333; background: #121216; color: #fff; }
  button { width: 100%; padding: 12px; background: #4338ca; color: white; border: none; border-radius: 6px; font-weight: bold; }
</style>
</head>
<body>
<h2>Konfigurasi Asisten</h2>
<form action="/save" method="POST">
  <label>SSID WiFi</label>
  <input type="text" name="wifi_ssid" required>
  <label>Password WiFi</label>
  <input type="password" name="wifi_pass">
  <label>API Key Gemini</label>
  <input type="text" name="api_key" required>
  <label>Nama Panggilan AI</label>
  <input type="text" name="ai_name" value="ESP">
  <label>Ingatan / Kepribadian AI</label>
  <textarea name="role_text" rows="6" placeholder="Nama saya Bima, istri Sari, anak Raka, suka kopi pahit..."></textarea>
  <button type="submit">Simpan</button>
</form>
</body>
</html>
)rawliteral";

// ================== NVS HELPERS ==================

static bool nvs_get_str_safe(const char *key, char *out, size_t max_len)
{
    nvs_handle_t handle;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = max_len;
    esp_err_t err = nvs_get_str(handle, key, out, &len);
    nvs_close(handle);
    return err == ESP_OK && len > 0;
}

static void nvs_set_str_safe(const char *key, const char *value)
{
    if (value == NULL) return;
    nvs_handle_t handle;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, key, value);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

// ================== HTTP HANDLERS ==================

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_FORM, strlen(HTML_FORM));
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char content[1024];
    int received = httpd_req_recv(req, content, sizeof(content) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Gagal menerima data");
        return ESP_FAIL;
    }
    content[received] = '\0';

    ESP_LOGI(TAG, "POST data: %s", content);

    // Parsing sederhana: key=value&key2=value2
    char wifi_ssid[64] = "";
    char wifi_pass[64] = "";
    char api_key[128] = "";
    char role_text[512] = "";

    char *token = strtok(content, "&");
    while (token) {
        char key[64] = "";
        char value[512] = "";
        if (sscanf(token, "%63[^=]=%511s", key, value) == 2) {
            if (strcmp(key, "wifi_ssid") == 0) {
                strncpy(wifi_ssid, value, sizeof(wifi_ssid) - 1);
            } else if (strcmp(key, "wifi_pass") == 0) {
                strncpy(wifi_pass, value, sizeof(wifi_pass) - 1);
            } else if (strcmp(key, "api_key") == 0) {
                strncpy(api_key, value, sizeof(api_key) - 1);
            } else if (strcmp(key, "role_text") == 0) {
                strncpy(role_text, value, sizeof(role_text) - 1);
            }
        }
        token = strtok(NULL, "&");
    }

    // Decode URL encoding sederhana (spasi +20%)
    // Ini tidak lengkap, tapi cukup untuk teks dasar.
    // Anda bisa gunakan lib curl/url decode jika tersedia.

    web_config_save(wifi_ssid, wifi_pass, api_key, role_text);
    ESP_LOGI(TAG, "Konfigurasi disimpan, restart...");

    httpd_resp_sendstr(req, "OK. Restart...");

    // Restart setelah 1 detik
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

// ================== PUBLIC API ==================

bool web_config_is_needed(void)
{
    nvs_handle_t handle;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return true;
    }
    size_t len = 0;
    esp_err_t err = nvs_get_str(handle, KEY_WIFI_SSID, NULL, &len);
    nvs_close(handle);
    return err != ESP_OK || len == 0;
}

void web_config_start(void)
{
    ESP_LOGI(TAG, "Memulai mode konfigurasi AP");

    // Init NVS
    nvs_flash_init();

    // WiFi AP
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, "ESP32-Config");
    ap_config.ap.ssid_len = strlen("ESP32-Config");
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // HTTP server
    httpd_config_t server_cfg = HTTPD_DEFAULT_CONFIG();
    server_cfg.server_port = 80;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &server_cfg) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t save_uri = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &save_uri);
        ESP_LOGI(TAG, "Web config server berjalan di http://192.168.4.1");
    } else {
        ESP_LOGE(TAG, "Gagal start HTTP server");
    }
}

void web_config_save(const char *wifi_ssid,
                     const char *wifi_pass,
                     const char *api_key,
                     const char *role_text)
{
    nvs_set_str_safe(KEY_WIFI_SSID, wifi_ssid);
    nvs_set_str_safe(KEY_WIFI_PASS, wifi_pass);
    nvs_set_str_safe(KEY_API_KEY, api_key);
    nvs_set_str_safe(KEY_ROLE_TEXT, role_text);
    nvs_set_str_safe(KEY_FORCE_CONFIG, "0");
}

bool web_config_load_role(char *buf, size_t max_len)
{
    return nvs_get_str_safe(KEY_ROLE_TEXT, buf, max_len);
}

void web_config_force_reset(void)
{
    nvs_set_str_safe(KEY_FORCE_CONFIG, "1");
    esp_restart();
}
