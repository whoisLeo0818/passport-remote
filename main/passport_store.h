#pragma once
#include "passport_model.h"
/* NVS must be initialized by the caller; never erases any namespace. */
bool passport_store_init(passport_settings_t *settings);
bool passport_store_save(passport_settings_t settings);
bool passport_store_failed(void);
