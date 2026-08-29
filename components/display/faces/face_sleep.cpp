#include "face_common.h"

void face_sleep_sequence(void) {
    face_render(0, 1, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
}
