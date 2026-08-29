#include "face_common.h"

void face_listening_sequence(void) {
    face_render(1, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(180));
}
