#include "face_common.h"

static int idle_shift_x = 0;
static int idle_shift_y = 0;
static int idle_expression = 0;
static TickType_t next_shift_tick = 0;
static bool idle_shift_initialized = false;

static void update_idle_shift() {
    TickType_t now = xTaskGetTickCount();
    if (!idle_shift_initialized || now >= next_shift_tick) {
        idle_shift_initialized = true;
        idle_shift_x = (int)face_rnd(7) - 3;
        idle_shift_y = (int)face_rnd(5) - 2;
        idle_expression = (int)face_rnd(3);
        next_shift_tick = now + pdMS_TO_TICKS(30000 + face_rnd(30001));
    }
}

static void idle_render(int step, int extra_x, int extra_y, int look) {
    update_idle_shift();
    face_render(idle_expression, step, idle_shift_x + extra_x, idle_shift_y + extra_y, look);
}

void face_idle_sequence(void) {
    update_idle_shift();
    vTaskDelay(pdMS_TO_TICKS(1500 + face_rnd(2001)));
    if (!face_is(FACE_IDLE)) return;
    uint32_t behavior = face_rnd(100);
    if (behavior < 30) {
        int x = (face_rnd(2) == 0) ? -4 : 4;
        int y = (int)face_rnd(7) - 3;
        face_smooth_move(idle_shift_x, idle_shift_y, idle_shift_x + x, idle_shift_y + y, 180, 0, FACE_IDLE);
        if (!face_is(FACE_IDLE)) return;
        vTaskDelay(pdMS_TO_TICKS(180 + face_rnd(121)));
        face_smooth_move(idle_shift_x + x, idle_shift_y + y, idle_shift_x, idle_shift_y, 180, 0, FACE_IDLE);
    } else if (behavior < 55) {
        int look = (face_rnd(2) == 0) ? 1 : 2;
        idle_render(0, 0, 0, look);
        vTaskDelay(pdMS_TO_TICKS(450 + face_rnd(401)));
        if (face_is(FACE_IDLE)) idle_render(0, 0, 0, 0);
    } else if (behavior < 70) {
        int look = (face_rnd(2) == 0) ? 1 : 2;
        int x = (look == 1) ? -3 : 3;
        int y = (look == 1) ? -2 : 2;
        face_smooth_move(idle_shift_x, idle_shift_y, idle_shift_x + x, idle_shift_y + y, 150, look, FACE_IDLE);
        if (!face_is(FACE_IDLE)) return;
        vTaskDelay(pdMS_TO_TICKS(200 + face_rnd(201)));
        face_smooth_move(idle_shift_x + x, idle_shift_y + y, idle_shift_x, idle_shift_y, 150, 0, FACE_IDLE);
    } else {
        face_blink(FACE_IDLE, face_rnd(5) == 0);
    }
}
