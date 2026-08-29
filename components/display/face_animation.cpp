#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "faces/face_common.h"

static const char *TAG = "FACE_ANIM";
static TaskHandle_t anim_task_handle = NULL;
static SemaphoreHandle_t anim_start_mutex = NULL;

static void state_sequence(face_state_t state) {
    switch (state) {
        case FACE_LISTENING: face_listening_sequence(); break;
        case FACE_THINKING:  face_thinking_sequence();  break;
        case FACE_SPEAKING:  face_speaking_sequence();  break;
        case FACE_HAPPY:     face_happy_sequence();     break;
        case FACE_SAD:       face_sad_sequence();       break;
        case FACE_ERROR:     face_error_sequence();     break;
        case FACE_SLEEP:     face_sleep_sequence();     break;
        case FACE_IDLE:
        default:             face_idle_sequence();     break;
    }
}

static void face_animation_task(void *arg) {
    (void)arg;
    oled_init();
    while (1) state_sequence(face_get_state());
}

void face_animation_start(void) {
    if (!anim_start_mutex) {
        anim_start_mutex = xSemaphoreCreateMutex();
        if (!anim_start_mutex) {
            ESP_LOGE(TAG, "Gagal membuat mutex animasi OLED");
            return;
        }
    }
    xSemaphoreTake(anim_start_mutex, portMAX_DELAY);
    if (!anim_task_handle) {
        BaseType_t ok = xTaskCreate(face_animation_task, "face_anim", 6144, NULL, 2, &anim_task_handle);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Gagal membuat task animasi OLED");
            anim_task_handle = NULL;
        } else {
            ESP_LOGI(TAG, "Mochi OLED animation aktif");
        }
    }
    xSemaphoreGive(anim_start_mutex);
}

void face_animation_stop(void) {
    if (!anim_start_mutex) return;
    xSemaphoreTake(anim_start_mutex, portMAX_DELAY);
    if (anim_task_handle) {
        vTaskDelete(anim_task_handle);
        anim_task_handle = NULL;
    }
    xSemaphoreGive(anim_start_mutex);
}
