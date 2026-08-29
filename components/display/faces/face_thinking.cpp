#include "face_common.h"

void face_thinking_sequence(void) {
    face_smooth_move(0, 0, -4, -2, 180, 1, FACE_THINKING);
    if (!face_is(FACE_THINKING)) return;
    vTaskDelay(pdMS_TO_TICKS(120));
    face_smooth_move(-4, -2, 4, -2, 360, 2, FACE_THINKING);
    if (!face_is(FACE_THINKING)) return;
    vTaskDelay(pdMS_TO_TICKS(120));
    face_smooth_move(4, -2, 0, 0, 180, 0, FACE_THINKING);
}
