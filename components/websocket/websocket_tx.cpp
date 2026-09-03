#include "websocket_internal.h"

#include "esp_log.h"

static const char *TAG = "WS_TX";

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
