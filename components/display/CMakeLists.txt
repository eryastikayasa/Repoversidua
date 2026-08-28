idf_component_register(
    SRCS "display.cpp" "face_animation.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd freertos
)
