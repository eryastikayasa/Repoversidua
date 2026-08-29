idf_component_register(
    SRCS
        "display.cpp"
        "face_animation.cpp"
        "faces/face_idle.cpp"
        "faces/face_listening.cpp"
        "faces/face_thinking.cpp"
        "faces/face_speaking.cpp"
        "faces/face_happy.cpp"
        "faces/face_sad.cpp"
        "faces/face_error.cpp"
        "faces/face_sleep.cpp"
    INCLUDE_DIRS "." "faces"
    REQUIRES driver esp_lcd freertos
)
