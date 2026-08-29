#include "face_common.h"

void face_speaking_sequence(void) {
    while (face_is(FACE_SPEAKING)) {
        face_render(2, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
        if (!face_is(FACE_SPEAKING)) break;
        face_render(2, 1, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(90));
        if (!face_is(FACE_SPEAKING)) break;
        face_render(2, 0, 0, 0, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
}
