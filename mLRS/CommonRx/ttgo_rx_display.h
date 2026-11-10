// TTGO T3 V1.6 receiver OLED. GPL-3.0, based on the mLRS project.
#pragma once
#ifdef RX_TTGO_T3_V16_868_ESP32

#include <stdio.h>
#include "ttgo_rx_display_state.h"
#define I2C_SDA_IO IO_P21
#define I2C_SCL_IO IO_P22
#define I2C_CLOCKSPEED 400000L
#include "../modules/esp-lib/esp-i2c.h"
#include "../Common/thirdparty/gdisp.h"

class tTtgoRxDisplay {
public:
    void Init(void) {
        if (started) return; // a parameter-store restart must not create a second I2C task
        i2c_init();
        // Do not notify the async worker unless its task was actually created.
        if (!i2c_async.task_handle) return;
        i2c_setdeviceadr(SSD1306_ADR);
        gdisp_init(GDISPLAY_TYPE_SSD1306);
        state.Init(millis32());
        last_draw_ms = millis32() - 500u;
        started = true;
    }
    void Receive(const uint8_t* data, uint16_t len) {
        state.Receive(data, len, millis32());
    }
    void Do(bool link, int rssi, int snr, unsigned lq, bool transparent) {
        if (!started) return;
        const uint32_t now = millis32();
        state.Tick(now);
        // Never change the framebuffer while the Core 0 I2C worker reads it.
        if (now - last_draw_ms < 500u || !gdisp_update_completed()) return;
        last_draw_ms = now;
        gdisp_clear();
        gdisp_unsetfont();
        gdisp_unsetinverted();
        char text[32];
        if (state.page == 0) {
            Line(6, "mLRS RX  STATUS 1/2");
            Line(20, link ? "VERBUNDEN" : "GETRENNT / SUCHE");
            if (link) {
                snprintf(text, sizeof(text), "RSSI %d dBm SNR %d", rssi, snr);
                Line(30, text);
                snprintf(text, sizeof(text), "LQ %u %%", lq);
                Line(40, text);
            } else {
                Line(30, "RSSI --    SNR --");
                Line(40, "LQ --");
            }
            snprintf(text, sizeof(text), "Textbytes: %lu", (unsigned long)state.bytes_received);
            Line(50, text);
            if (state.has_text) {
                snprintf(text, sizeof(text), "Text vor %lu s", (unsigned long)((now - state.last_text_ms) / 1000u));
                Line(60, text);
            } else Line(60, "Noch kein Text");
        } else {
            Line(6, link ? "TEXT 2/2   VERBUNDEN" : "TEXT 2/2   GETRENNT");
            if (!transparent) {
                Line(20, "Text nur im Modus");
                Line(30, "transparent");
            } else if (!state.has_text) {
                Line(20, "Warte auf Funktext...");
            } else {
                for (unsigned i = 0; i < tTtgoRxDisplayState::Rows; ++i)
                    Line(20 + i * 10, state.lines[i]);
            }
        }
        gdisp_drawline_H(0, 10, 128, 1);
        gdisp_update(); // async I2C transfer on Core 0, no delay in radio loop
    }
private:
    bool started = false;
    uint32_t last_draw_ms = 0;
    tTtgoRxDisplayState state;
    void Line(int y, const char* text) {
        // Bound all output to 21 ASCII glyphs (128 px at 6 px/glyph).
        char bounded[tTtgoRxDisplayState::Columns + 1];
        strncpy(bounded, text, sizeof(bounded) - 1);
        bounded[sizeof(bounded) - 1] = '\0';
        gdisp_setcurXY(0, y);
        gdisp_puts(bounded);
    }
};
static tTtgoRxDisplay ttgo_rx_display;
#endif
