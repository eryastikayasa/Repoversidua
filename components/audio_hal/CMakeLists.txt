idf_component_register(
    SRCS "audio_hal.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_timer
)
