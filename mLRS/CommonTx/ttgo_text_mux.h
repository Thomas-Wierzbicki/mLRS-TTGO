// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <string.h>

// Main-loop only. Bounded UART work, complete lines, fair source selection.
class TtgoTextMux {
public:
    enum { max_text = 255, depth = 4, capacity = max_text + 2 };
    void reset() {
        head = tail = count = used = pos = length = 0;
        discard = false;
        prefer_usb = true;
    }
    template<class Serial, class Sensor>
    bool available(Serial& serial, Sensor sensor) {
        // Stop consuming UART when our queue is full. Hardware UART buffering
        // is finite too: this is a text terminal, not a bulk binary transport.
        for (unsigned budget = 0; budget < 32 && count < depth && serial.available(); ++budget) {
            const unsigned char c = (unsigned char)serial.getc();
            if (c == '\n') {
                if (!discard && used) {
                    sizes[tail] = used;
                    memcpy(lines[tail], partial, used);
                    tail = (tail + 1) % depth;
                    ++count;
                }
                used = 0; discard = false;
            } else if (c == '\r') {
                continue;
            } else if (!discard) {
                if (used == max_text || c == 0) { discard = true; used = 0; }
                else partial[used++] = (char)c; // Preserve UTF-8 bytes.
            }
        }
        if (pos < length) return true; // Never switch mid-record.
        pos = length = 0;
        if (prefer_usb && count) return select_usb();
        if (sensor(active, length)) {
            prefer_usb = true;
            return length != 0;
        }
        if (count) return select_usb();
        return false;
    }
    char getc() { return pos < length ? active[pos++] : 0; }
private:
    bool select_usb() {
        active[0] = '\n';
        memcpy(active + 1, lines[head], sizes[head]);
        length = sizes[head] + 2;
        active[length - 1] = '\n';
        head = (head + 1) % depth;
        --count;
        prefer_usb = false; // Give a waiting sensor record the next turn.
        return true;
    }
    char partial[max_text] = {};
    char lines[depth][max_text] = {};
    uint16_t sizes[depth] = {};
    char active[capacity] = {};
    uint16_t head = 0, tail = 0, count = 0, used = 0, pos = 0, length = 0;
    bool discard = false, prefer_usb = true;
};
