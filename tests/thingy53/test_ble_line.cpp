#include "../../mLRS/CommonTx/ttgo_ble_line.h"
#include <cassert>
#include <string>
#include <vector>
int main() {
    const std::string input = "\nT:22.6C H:38% P:1004hPa\n";
    for (size_t split = 0; split <= input.size(); ++split) {
        TtgoBleLine p;
        std::vector<std::string> got;
        auto emit = [&](const char* s, size_t n) { got.emplace_back(s,n); };
        p.feed((const unsigned char*)input.data(), split, emit);
        p.feed((const unsigned char*)input.data()+split, input.size()-split, emit);
        assert(got.size() == 1 && got[0] == "T:22.6C H:38% P:1004hPa");
    }
    TtgoBleLine p;
    std::vector<std::string> got;
    auto feed = [&](std::string s) { p.feed((const unsigned char*)s.data(), s.size(), [&](const char* v, size_t n) { got.emplace_back(v,n); }); };
    feed("orphan\n\nOK\r\n");
    assert(got.size() == 1 && got.back() == "OK");
    feed(std::string(1000, 'a') + "\nNEXT\n");
    assert(got.size() == 2 && got.back() == "NEXT");
    feed("partial"); p.reset(); feed("tail\nNEW\n");
    assert(got.size() == 3 && got.back() == "NEW");
    feed(std::string("bad\x01record\nVALID\n"));
    assert(got.size() == 4 && got.back() == "VALID");
}
