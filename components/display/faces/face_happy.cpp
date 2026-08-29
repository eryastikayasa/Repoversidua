#include "face_common.h"

void face_happy_sequence(void) {
    face_render(2, 0, 0, -1, 0);
    vTaskDelay(pdMS_TO_TICKS(140));
    if (!face_is(FACE_HAPPY)) return;
    face_render(2, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(140));
    if (face_is(FACE_HAPPY)) face_set_state(FACE_LISTENING);
}
