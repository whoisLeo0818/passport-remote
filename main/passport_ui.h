#pragma once
#include "passport_ble.h"
bool passport_ui_init(void);
/* Requires LVGL lock. Drop objects/cache before Flash banks/descriptors change. */
void passport_ui_clear_assets(void);
/* All calls require the BSP LVGL lock. The view has application lifetime. */
void passport_ui_refresh(const passport_model_t *model, remote_status_t remote,
                         int battery, const char *feedback);
