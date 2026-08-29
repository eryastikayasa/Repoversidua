idf_component_register(
    SRCS "audio_hal.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2s freertos esp_timer
)
