#include "websocket_internal.h"
#include "web_config.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"
#include <stdio.h>

static const char *TAG = "WS_TX";

/* Runtime WebSocket URL. API key berasal dari NVS/web_config. */
const char *websocket_get_server_url(void)
{
    static char url[256];
    char api_key[128] = {0};

    if (!web_config_load_api_key(api_key, sizeof(api_key))) {
        ESP_LOGE(TAG, "Gemini API key tidak ditemukan di NVS");
        return NULL;
    }

    int n = snprintf(
        url, sizeof(url),
        "wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent?key=%s",
        api_key);

    if (n < 0 || (size_t)n >= sizeof(url)) {
        ESP_LOGE(TAG, "WebSocket URL terlalu panjang");
        return NULL;
    }

    return url;
}

void websocket_send_audio_data(
    const uint8_t *data,
    size_t len
)
{
    if (!data || len == 0) {
        return;
    }

    if (len > WS_TX_AUDIO_SIZE) {
        ESP_LOGE(
            TAG,
            "Audio frame terlalu besar: %u byte",
            (unsigned)len
        );
        return;
    }

    if (!is_connected ||
        !setup_complete ||
        websocket_tx_error) {
        return;
    }

    uint32_t generation =
        websocket_connection_generation;

    if (!websocket_tx_enqueue_audio(
            data,
            len,
            generation)) {
        ESP_LOGD(
            TAG,
            "Audio frame tidak masuk TX queue"
        );
    }
}

/*
 * Gemini Live API meminta tool response kembali melalui WebSocket.
 * Tool response dikirim setelah eksekusi command selesai.
 */
bool websocket_send_tool_response(const char *id, const char *name, bool success)
{
    if (!id || !name || !client || !is_connected || !setup_complete || websocket_tx_error) {
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *tool_response = cJSON_CreateObject();
    cJSON *function_responses = cJSON_CreateArray();
    cJSON *function_response = cJSON_CreateObject();
    cJSON *response = cJSON_CreateObject();

    if (!root || !tool_response || !function_responses ||
        !function_response || !response) {
        cJSON_Delete(root);
        cJSON_Delete(tool_response);
        cJSON_Delete(function_responses);
        cJSON_Delete(function_response);
        cJSON_Delete(response);
        return false;
    }

    cJSON_AddStringToObject(function_response, "id", id);
    cJSON_AddStringToObject(function_response, "name", name);
    cJSON_AddStringToObject(response, "result", success ? "ok" : "error");
    cJSON_AddItemToObject(function_response, "response", response);
    cJSON_AddItemToArray(function_responses, function_response);
    cJSON_AddItemToObject(tool_response, "functionResponses", function_responses);
    cJSON_AddItemToObject(root, "toolResponse", tool_response);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) {
        return false;
    }

    if (!client || !is_connected || !setup_complete || websocket_tx_error) {
        free(json);
        return false;
    }

    int json_len = (int)strlen(json);
    int sent = esp_websocket_client_send_text(
        client,
        json,
        json_len,
        pdMS_TO_TICKS(3000));

    bool ok = sent == json_len;
    ESP_LOGI(TAG, "Tool response %s: id=%s name=%s sent=%d/%d",
             ok ? "terkirim" : "gagal", id, name, sent, json_len);

    free(json);
    return ok;
}
