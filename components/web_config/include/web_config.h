#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cek apakah perlu masuk mode konfigurasi.
// Return true jika NVS kosong / force_config = 1.
bool web_config_is_needed(void);

// Mulai Access Point dan HTTP server untuk konfigurasi.
void web_config_start(void);

// Simpan konfigurasi ke NVS.
void web_config_save(const char *wifi_ssid,
                     const char *wifi_pass,
                     const char *api_key,
                     const char *role_text);

// Muat konfigurasi dari NVS. Digunakan oleh build_gemini_setup.
// Return true jika data ditemukan.
bool web_config_load_role(char *buf, size_t max_len);

// Set force_config = 1, lalu restart (untuk masuk mode config lagi)
void web_config_force_reset(void);

#ifdef __cplusplus
}
#endif
