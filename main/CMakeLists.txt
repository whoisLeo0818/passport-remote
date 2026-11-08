idf_component_register(
    SRCS "main.c" "passport_ui.c" "passport_store.c" "passport_model.c" "passport_ble.c"
         "passport_wifi.c" "passport_assets.c" "passport_transfer.c"
         "passport_weather.c" "passport_weather_model.c"
         "../assets/images/passport/passport_photos.c"
         "../assets/fonts/passport_font_12.c"
         "../assets/fonts/passport_font_16.c"
         "../assets/fonts/passport_font_20.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_timer nvs_flash esp_wifi esp_netif esp_event esp_http_server esp_partition json esp_http_client mbedtls
    EMBED_FILES "passport_web.html"
)

# Keep the deployment credential out of tracked sources/configuration.
# CI/users may supply PASSPORT_WIFI_PASSWORD through the environment, or an
# ignored checkout-local build/tooling/passport_wifi_password.txt file.
set(passport_password "$ENV{PASSPORT_WIFI_PASSWORD}")
if(NOT passport_password AND EXISTS "${CMAKE_SOURCE_DIR}/build/tooling/passport_wifi_password.txt")
    file(READ "${CMAKE_SOURCE_DIR}/build/tooling/passport_wifi_password.txt" passport_password)
    string(STRIP "${passport_password}" passport_password)
endif()
string(LENGTH "${passport_password}" passport_password_length)
if(passport_password_length LESS 8 OR passport_password_length GREATER 63 OR
   NOT passport_password MATCHES "^[A-Za-z0-9@._!+-]+$")
    message(FATAL_ERROR "Set PASSPORT_WIFI_PASSWORD to an 8-63 character deployment password (letters/digits/@._!+-).")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/passport_wifi_secret.h" "#pragma once\n#define PASSPORT_WIFI_PASSWORD \"${passport_password}\"\n")
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
