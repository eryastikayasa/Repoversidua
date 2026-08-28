idf_component_register(
    SRCS
        "websocket_audio.cpp"
        "websocket_events.cpp"
        "websocket_json.cpp"
        "websocket_mgr.cpp"
        "websocket_rx.cpp"
        "websocket_stack_wrap.c"
        "websocket_tx.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_websocket_client
        cjson
        audio_hal
        display
        wifi_manager
        freertos
)
