#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================
// GEMINI LIVE API WEBSOCKET
// ============================================================
// URL akan dibangun secara dinamis dari API key yang tersimpan di NVS.
// Tidak ada API key hardcoded di sini.

void websocket_app_start(void);

void websocket_send_audio_data(const uint8_t *data, size_t len);

bool websocket_is_connected(void);
