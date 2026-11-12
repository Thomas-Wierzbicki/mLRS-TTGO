// TTGO receiver OLED state. GPL-3.0, based on the mLRS project.
#pragma once
#include <stdint.h>
#include <string.h>

class tTtgoRxDisplayState {
public:
    static constexpr unsigned Rows = 5, Columns = 21;
    char lines[Rows][Columns + 1] = {};
    bool has_text = false;
    uint32_t bytes_received = 0, last_text_ms = 0;
    uint8_t page = 0;

    void Init(uint32_t now) { page = 0; page_ms = now; }
    void Tick(uint32_t now) {
        uint32_t elapsed = now - page_ms;
        if (elapsed >= 15000u) {
            uint32_t steps = elapsed / 15000u;
            page ^= (steps & 1u);
            page_ms += steps * 15000u;
        }
    }
    void Receive(const uint8_t* data, uint16_t len, uint32_t now) {
        bytes_received += len;
        for (uint16_t i = 0; i < len; ++i) Decode(data[i], now);
    }
private:
    uint32_t page_ms = 0, unicode = 0, unicode_min = 0;
    unsigned row = 0, column = 0;
    uint8_t continuation = 0;
    bool new_line = false;

    void Append(char c, uint32_t now) {
        if (new_line || column == Columns) {
            if (row + 1 < Rows) ++row;
            else {
                memmove(lines[0], lines[1], (Rows - 1) * (Columns + 1));
                memset(lines[Rows - 1], 0, Columns + 1);
            }
            column = 0;
            new_line = false;
        }
        // Compact the telemetry prefix only on the OLED. UART keeps the raw record.
        if (c == ' ' && column == 19 && !memcmp(lines[row], "ID=", 3) &&
            strspn(lines[row] + 3, "0123456789ABCDEF") == 16) {
            memmove(lines[row], lines[row] + 13, 6);
            column = 6;
        }
        lines[row][column++] = c;
        lines[row][column] = '\0';
        has_text = true;
        last_text_ms = now;
    }
    void Codepoint(uint32_t c, uint32_t now) {
        const char* replacement = nullptr;
        switch (c) {
        case 0xc4: replacement = "Ae"; break;
        case 0xd6: replacement = "Oe"; break;
        case 0xdc: replacement = "Ue"; break;
        case 0xe4: replacement = "ae"; break;
        case 0xf6: replacement = "oe"; break;
        case 0xfc: replacement = "ue"; break;
        case 0xdf: replacement = "ss"; break;
        }
        if (replacement) { Append(replacement[0], now); Append(replacement[1], now); }
        else Append('?', now);
    }
    void Decode(uint8_t c, uint32_t now) {
        if (continuation) {
            if ((c & 0xc0) == 0x80) {
                unicode = (unicode << 6) | (c & 0x3f);
                if (!--continuation) {
                    if (unicode < unicode_min || unicode > 0x10ffff ||
                        (unicode >= 0xd800 && unicode <= 0xdfff)) Append('?', now);
                    else Codepoint(unicode, now);
                }
                return;
            }
            continuation = 0;
            Append('?', now); // reprocess the non-continuation byte below
        }
        if (c >= 0x20 && c <= 0x7e) { Append((char)c, now); return; }
        if (c == '\r' || c == '\n') { if (column) new_line = true; return; }
        if (c == '\t') { Append(' ', now); return; }
        if (c < 0x80) return; // ignore binary control characters on the OLED only
        if (c >= 0xc2 && c <= 0xdf) { unicode = c & 0x1f; unicode_min = 0x80; continuation = 1; }
        else if (c >= 0xe0 && c <= 0xef) { unicode = c & 0x0f; unicode_min = 0x800; continuation = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { unicode = c & 7; unicode_min = 0x10000; continuation = 3; }
        else Append('?', now);
    }
};
