#include "face_common.h"

void face_error_sequence(void) {
    face_smooth_move(0, 0, 1, 0, 120, 0, FACE_ERROR);
    if (!face_is(FACE_ERROR)) return;
    face_render(99, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(250));
}
