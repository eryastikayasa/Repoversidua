#include "face_common.h"

void face_sad_sequence(void) {
    face_smooth_move(0, 0, 0, 2, 120, 0, FACE_SAD);
    if (!face_is(FACE_SAD)) return;
    face_render(6, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(250));
}
