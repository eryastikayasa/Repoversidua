#include "websocket_internal.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "WS_RX";

// RX slots are only transport staging buffers.  They must be released as
// soon as the WebSocket callback has copied the frame out of the slot.
// JSON/cJSON/audio processing is intentionally done on a separate heap copy.
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

static uint8_t *alloc_process_buffer(size_t len)
{
    // Keep the processing copy in PSRAM so RX slots and internal RAM remain
    // available for the WebSocket/I2S scheduler.
    uint8_t *p = static_cast<uint8_t *>(
        heap_caps_malloc(len + WS_RX_TERMINATOR_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!p) {
        p = static_cast<uint8_t *>(
            heap_caps_malloc(len + WS_RX_TERMINATOR_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    return p;
}

static void websocket_rx_task(void *)
{
    ws_rx_command_t cmd{};

    for (;;) {
        if (xQueueReceive(websocket_rx_queue, &cmd, portMAX_DELAY) != pdTRUE)
            continue;

        if (cmd.buffer != nullptr && cmd.len > 0) {
            // process_gemini_message() is length-aware and performs the
            // compact-audio fast path before cJSON parsing.
            process_gemini_message(reinterpret_cast<const char *>(cmd.buffer), cmd.len);
        }

        if (cmd.buffer != nullptr) {
            heap_caps_free(cmd.buffer);
            cmd.buffer = nullptr;
        }
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
            WS_RX_SLOT_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!s_slots[i].buffer) {
            // Fallback only if PSRAM allocation is unavailable.
            s_slots[i].buffer = static_cast<uint8_t *>(heap_caps_malloc(
                WS_RX_SLOT_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        }
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

    ESP_LOGI(TAG, "RX pool ready: %u slots x %u bytes, task priority=7, slot release=immediate",
             WS_RX_SLOT_COUNT, WS_RX_SLOT_SIZE);
    return true;
}

bool websocket_rx_enqueue_data(esp_websocket_event_data_t *data, uint32_t generation)
{
    if (!data || !data->data_ptr || data->data_len <= 0 || !websocket_rx_queue)
        return false;

    // Reserve one byte for the NUL terminator used by the compact-audio
    // fast path (strstr/memmove). Keep the existing 64 KB slot size.
    if ((size_t)data->data_len >= WS_RX_SLOT_SIZE)
        return false;

    uint8_t slot_id = 0;
    if (!slot_take(&slot_id)) {
        ESP_LOGW(TAG, "RX pool full: dropping frame len=%d", data->data_len);
        return false;
    }

    rx_slot_t &slot = s_slots[slot_id];
    memcpy(slot.buffer, data->data_ptr, data->data_len);
    slot.buffer[data->data_len] = '\0';

    // Make a PSRAM processing copy, then release the transport slot BEFORE
    // JSON/cJSON/audio work starts. This is the key change that prevents the
    // WebSocket callback from exhausting the RX slot pool during bursts.
    uint8_t *process_buffer = alloc_process_buffer((size_t)data->data_len);
    if (!process_buffer) {
        slot_release(slot_id);
        ESP_LOGW(TAG, "RX process buffer allocation failed: len=%d", data->data_len);
        return false;
    }
    memcpy(process_buffer, slot.buffer, (size_t)data->data_len + WS_RX_TERMINATOR_SIZE);
    slot_release(slot_id);

    ws_rx_command_t cmd{};
    cmd.generation = generation;
    cmd.buffer = process_buffer;
    cmd.len = (uint32_t)data->data_len;
    cmd.slot_id = 0;

    if (xQueueSend(websocket_rx_queue, &cmd, 0) != pdTRUE) {
        heap_caps_free(process_buffer);
        ESP_LOGW(TAG, "RX processing queue full: dropping frame len=%d", data->data_len);
        return false;
    }

    return true;
}

void websocket_rx_flush_queue(void)
{
    if (!websocket_rx_queue) return;

    ws_rx_command_t cmd{};
    while (xQueueReceive(websocket_rx_queue, &cmd, 0) == pdTRUE) {
        if (cmd.buffer) heap_caps_free(cmd.buffer);
    }
}

void websocket_rx_request_reset(void)
{
    websocket_rx_flush_queue();
}
