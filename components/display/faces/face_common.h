#pragma once

#include "display.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static inline uint32_t face_rnd(uint32_t max_value) {
    return max_value ? esp_random() % max_value : 0;
}

static inline void face_render(int expr, int step, int sx, int sy, int look) {
    display_render_mochi(expr, step, sx, sy, look);
}

static inline bool face_is(face_state_t state) {
    return face_get_state() == state;
}

static inline void face_smooth_move(int from_x, int from_y, int to_x, int to_y,
                                    uint32_t duration_ms, int look, face_state_t state) {
    const int step_ms = 30;
    int steps = (int)(duration_ms / step_ms);
    if (steps < 1) steps = 1;
    int expr = 0;
    if (state == FACE_HAPPY || state == FACE_LISTENING) expr = 1;
    else if (state == FACE_SAD) expr = 6;
    else if (state == FACE_ERROR) expr = 99;
    else if (state == FACE_SPEAKING) expr = 2;
    for (int i = 1; i <= steps; ++i) {
        if (!face_is(state)) return;
        int x = from_x + ((to_x - from_x) * i) / steps;
        int y = from_y + ((to_y - from_y) * i) / steps;
        face_render(expr, 0, x, y, look);
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }
}

static inline void face_blink(face_state_t state, bool double_blink) {
    int expr = (state == FACE_HAPPY || state == FACE_LISTENING) ? 2 :
               (state == FACE_SAD ? 6 : 0);
    if (!face_is(state)) return;
    face_render(expr, 1, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!face_is(state)) return;
    face_render(expr, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    if (double_blink && face_is(state)) {
        face_render(expr, 1, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (face_is(state)) face_render(expr, 0, 0, 0, 0);
    }
}

void face_idle_sequence(void);
void face_listening_sequence(void);
void face_thinking_sequence(void);
void face_speaking_sequence(void);
void face_happy_sequence(void);
void face_sad_sequence(void);
void face_error_sequence(void);
void face_sleep_sequence(void);
