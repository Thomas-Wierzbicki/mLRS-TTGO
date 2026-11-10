// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
// Notifications can split anywhere. Discard incomplete/oversized records.
// After connection/reset, synchronize on a newline before accepting data.
class TtgoBleLine {
public:
    enum { capacity = 96 };
    void reset() { used = 0; discard = true; }
    template<class Emit> void feed(const unsigned char* data, size_t len, Emit emit) {
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = data[i];
            if (c == '\n') {
                if (!discard && used) { line[used] = 0; emit(line, used); }
                used = 0; discard = false;
            } else if (c == '\r') {
                continue;
            } else if (!discard) {
                if (c < 32 || c > 126 || used >= capacity - 1) {
                    discard = true; used = 0;
                } else { line[used++] = (char)c; }
            }
        }
    }
private:
    char line[capacity] = {};
    size_t used = 0;
    bool discard = true;
};
