#include "websocket_internal.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "WS_RX";

// RX slot ownership is deliberately kept separate from queue ownership.
// A slot is returned immediately after the queued command has been consumed.
// This prevents the RX pool from being held by slow downstream processing.
struct rx_slot_t {
    uint8_t *buffer;
    bool in_use;
};

static rx_slot_t *s_slots = nullptr;
static StaticSemaphore_t s_slot_mutex_buf;
static SemaphoreHandle_t s_slot_mutex = nullptr;
static uint8_t s_next_slot = 0;

static bool slot_take(uint8_t *slot_id)
{
    if (!s_slots || !s_slot_mutex || !slot_id) return false;

    if (xSemaphoreTake(s_slot_mutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;

    for (uint8_t n = 0; n < WS_RX_SLOT_COUNT; ++n) {
        const uint8_t id = (uint8_t)((s_next_slot + n) % WS_RX_SLOT_COUNT);
        if (!s_slots[id].in_use) {
            s_slots[id].in_use = true;
            s_next_slot = (uint8_t)((id + 1) % WS_RX_SLOT_COUNT);
            *slot_id = id;
            xSemaphoreGive(s_slot_mutex);
            return true;
        }
    }

    xSemaphoreGive(s_slot_mutex);
    return false;
}

static void slot_release(uint8_t slot_id)
{
    if (!s_slots || !s_slot_mutex || slot_id >= WS_RX_SLOT_COUNT) return;

    if (xSemaphoreTake(s_slot_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        s_slots[slot_id].in_use = false;
        xSemaphoreGive(s_slot_mutex);
    }
}

static void websocket_rx_task(void *)
{
    ws_rx_command_t cmd{};

    for (;;) {
        if (xQueueReceive(websocket_rx_queue, &cmd, portMAX_DELAY) != pdTRUE)
            continue;

        // Copy all information needed by the processor before releasing the slot.
        // process_websocket_payload() must not retain cmd.buffer after returning.
        process_websocket_payload(reinterpret_cast<esp_websocket_event_data_t *>(&cmd));

        // CRITICAL: release immediately after processing one frame.
        // Never wait for the next WebSocket frame, audio playback, or JSON response.
        slot_release(cmd.slot_id);
    }
}

bool websocket_rx_init(void)
{
    if (websocket_rx_queue) return true;

    s_slots = static_cast<rx_slot_t *>(heap_caps_calloc(
        WS_RX_SLOT_COUNT, sizeof(rx_slot_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!s_slots) {
        ESP_LOGE(TAG, "RX slot metadata allocation failed");
        return false;
    }

    s_slot_mutex = xSemaphoreCreateMutexStatic(&s_slot_mutex_buf);
    if (!s_slot_mutex) return false;

    for (uint8_t i = 0; i < WS_RX_SLOT_COUNT; ++i) {
        s_slots[i].buffer = static_cast<uint8_t *>(heap_caps_malloc(
            WS_RX_SLOT_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (!s_slots[i].buffer) {
            ESP_LOGE(TAG, "RX slot %u allocation failed", i);
            for (uint8_t j = 0; j < i; ++j) heap_caps_free(s_slots[j].buffer);
            heap_caps_free(s_slots);
            s_slots = nullptr;
            return false;
        }
    }

    websocket_rx_queue = xQueueCreate(WS_RX_QUEUE_LENGTH, sizeof(ws_rx_command_t));
    if (!websocket_rx_queue) {
        ESP_LOGE(TAG, "RX queue allocation failed");
        return false;
    }

    BaseType_t ok = xTaskCreate(websocket_rx_task, "websocket_rx", 6144, nullptr, 7,
                                &websocket_rx_task_handle);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "RX task creation failed");
        vQueueDelete(websocket_rx_queue);
        websocket_rx_queue = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "RX pool ready: %u slots x %u bytes, task priority=7",
             WS_RX_SLOT_COUNT, WS_RX_SLOT_SIZE);
    return true;
}

bool websocket_rx_enqueue_data(esp_websocket_event_data_t *data, uint32_t generation)
{
    if (!data || !data->data_ptr || data->data_len <= 0 || !websocket_rx_queue)
        return false;

    if ((size_t)data->data_len > WS_RX_SLOT_SIZE)
        return false;

    uint8_t slot_id = 0;
    if (!slot_take(&slot_id)) {
        ESP_LOGW(TAG, "RX pool full: dropping frame len=%d", data->data_len);
        return false;
    }

    memcpy(s_slots[slot_id].buffer, data->data_ptr, data->data_len);

    ws_rx_command_t cmd{};
    cmd.generation = generation;
    cmd.buffer = s_slots[slot_id].buffer;
    cmd.len = (uint32_t)data->data_len;
    cmd.slot_id = slot_id;

    // Queue only owns the small descriptor. The slot remains occupied until
    // websocket_rx_task finishes processing this exact frame.
    if (xQueueSend(websocket_rx_queue, &cmd, 0) != pdTRUE) {
        slot_release(slot_id);
        return false;
    }

    return true;
}

void websocket_rx_flush_queue(void)
{
    if (!websocket_rx_queue) return;

    ws_rx_command_t cmd{};
    while (xQueueReceive(websocket_rx_queue, &cmd, 0) == pdTRUE) {
        slot_release(cmd.slot_id);
    }
}

void websocket_rx_request_reset(void)
{
    websocket_rx_flush_queue();
}
