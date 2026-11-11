// SPDX-License-Identifier: GPL-3.0-or-later
// Only built by tx-ttgo-t3-v16-868-thingy53. No BLE calls in the radio loop.
#if defined(TTGO_THINGY53_BLE)
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include "ttgo_ble_bridge.h"
#include "ttgo_ble_line.h"

namespace {
const char* const nus = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* const txchar = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
struct Record { char text[100]; uint16_t len; TickType_t received; };
struct Peer {
    NimBLEClient* client = nullptr;
    QueueHandle_t latest = nullptr;
    TtgoBleLine parser;
    std::atomic<uint32_t> last{0};
};
Peer peers[2];
const char* const names[] = {"Thingy53_mLRS","Dongle_1"};
unsigned next_source = 0;
TaskHandle_t worker = nullptr;

void publish(Peer& peer, const char* text, size_t length) {
    Record r = {};
    if (!peer.latest || !length || length > 95) return;
    // Leading newline recovers receiver framing after a radio interruption.
    r.text[0] = '\n';
    memcpy(r.text + 1, text, length);
    r.text[length + 1] = '\n';
    r.len = length + 2;
    r.received = xTaskGetTickCount();
    xQueueOverwrite(peer.latest, &r); // Never accumulate stale sensor history.
}
class Callbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient* client) override {
        for (unsigned i = 0; i < 2; ++i) {
            if (peers[i].client != client) continue;
            peers[i].parser.reset();
            const char* message = i == 0 ? "BLE:offline" : "D1:offline";
            publish(peers[i], message, strlen(message));
        }
    }
};
Callbacks callbacks;

void subscribe_peer(unsigned i, NimBLEAdvertisedDevice& device) {
    Peer& peer = peers[i];
    if (!peer.client->connect(&device)) return;
    auto* svc = peer.client->getService(nus);
    auto* ch = svc ? svc->getCharacteristic(txchar) : nullptr;
    peer.last.store(millis());
    auto receive = [i](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t size, bool) {
        Peer& source = peers[i];
        source.last.store(millis());
        source.parser.feed(bytes, size, [&source](const char* line, size_t len) {
            publish(source, line, len);
        });
    };
    if (!ch || !ch->canNotify() || !ch->subscribe(true, receive, true)) peer.client->disconnect();
}

void run(void*) {
    NimBLEDevice::init("TTGO_mLRS_TX");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true); // Name is in Thingy's scan response.
    scan->setInterval(160);
    scan->setWindow(40);
    for (auto& peer : peers) {
        peer.client = NimBLEDevice::createClient();
        if (!peer.client) continue;
        peer.client->setClientCallbacks(&callbacks, false);
        peer.client->setConnectTimeout(5);
        peer.client->setConnectionParams(24, 40, 0, 400);
    }
    for (;;) {
        bool search = false;
        for (auto& peer : peers) {
            if (!peer.client) continue;
            if (!peer.client->isConnected()) search = true;
            else if ((uint32_t)(millis() - peer.last.load()) > 20000) peer.client->disconnect();
        }
        if (search) {
            NimBLEScanResults results = scan->start(3, false);
            for (int n = 0; n < results.getCount(); ++n) {
                auto device = results.getDevice(n);
                for (unsigned i = 0; i < 2; ++i) {
                    if (!peers[i].client || peers[i].client->isConnected()) continue;
                    if (device.getName() == names[i] && device.isAdvertisingService(NimBLEUUID(nus)))
                        subscribe_peer(i, device);
                }
            }
            scan->clearResults();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}

void ttgo_ble_init() {
    if (worker) return; // mLRS can reinitialize its configuration without reboot.
    for (auto& peer : peers) {
        if (!peer.latest) peer.latest = xQueueCreate(1, sizeof(Record));
        if (!peer.latest) return;
    }
    if (xTaskCreatePinnedToCore(run, "thingy_ble", 6144, nullptr, 1, &worker, 0) != pdPASS) {
        worker = nullptr;
        publish(peers[0], "BLE:task error", 14);
    }
}
bool ttgo_ble_take(char* text, uint16_t& length) {
    Record r = {};
    length = 0;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        unsigned i = (next_source + attempt) % 2;
        auto queue = peers[i].latest;
        if (!queue || xQueueReceive(queue, &r, 0) != pdTRUE) continue;
        if ((TickType_t)(xTaskGetTickCount() - r.received) > pdMS_TO_TICKS(15000)) continue;
        memcpy(text, r.text, r.len);
        length = r.len;
        next_source = 1 - i;
        return true;
    }
    return false;
}
void ttgo_ble_flush() {
    for (auto& peer : peers) if (peer.latest) xQueueReset(peer.latest);
}

#endif
