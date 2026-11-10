// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
void ttgo_ble_init();
// Caller provides at least 100 bytes. Returns one complete framed record.
bool ttgo_ble_take(char* text, uint16_t& length);
void ttgo_ble_flush();
