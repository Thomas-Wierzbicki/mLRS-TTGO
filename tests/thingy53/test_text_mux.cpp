#include "../../mLRS/CommonTx/ttgo_text_mux.h"
#include <cassert>
#include <string>
#include <deque>
struct Serial {
    std::string input;
    bool available() { return !input.empty(); }
    char getc() { char c = input[0]; input.erase(0,1); return c; }
};
int main() {
    TtgoTextMux mux; Serial serial;
    std::deque<std::string> sensor;
    auto take = [&](char* out, uint16_t& n) {
        if (sensor.empty()) { n = 0; return false; }
        n = sensor.front().size(); memcpy(out, sensor.front().data(), n); sensor.pop_front(); return true;
    };
    auto drain = [&]() {
        std::string result;
        for (int i=0; i<10000; ++i) {
            if (mux.available(serial,take)) result += mux.getc();
            else if (!serial.available() && sensor.empty()) return result;
        }
        assert(false); return result;
    };
    serial.input="Hallo\nWelt\r\n"; sensor={"\nT:22.6C\n"};
    assert(drain()=="\nHallo\n\nT:22.6C\n\nWelt\n");
    serial.input="angefangen"; sensor={"\nH:38%\n"};
    assert(drain()=="\nH:38%\n");
    serial.input=" fertig\n"; assert(drain()=="\nangefangen fertig\n");
    serial.input="Gr\xc3\xbc\xc3\x9f" "e\n";
    assert(drain()=="\nGr\xc3\xbc\xc3\x9f" "e\n");
    serial.input=std::string(256,'x')+"\nOK\n";
    assert(drain()=="\nOK\n");
    serial.input=std::string(255,'x')+"\n";
    assert(drain()=="\n"+std::string(255,'x')+"\n");
    serial.input="\n\r\n"; assert(drain().empty());
    serial.input="ABCDEF\n";
    assert(mux.available(serial,take)); assert(mux.getc()=='\n');
    sensor={"\nP:1004\n"};
    assert(drain()=="ABCDEF\n\nP:1004\n");
    serial.input="unfinished"; drain(); mux.reset(); serial.input="new\n";
    assert(drain()=="\nnew\n");
    serial.input.clear(); std::string expected;
    for(int i=0;i<20;++i) { serial.input+=std::to_string(i)+"\n"; expected+="\n"+std::to_string(i)+"\n"; }
    assert(drain()==expected); // queue wraparound and backpressure
}
