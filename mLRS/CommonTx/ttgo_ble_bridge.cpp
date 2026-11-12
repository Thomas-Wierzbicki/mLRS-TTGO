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
constexpr unsigned peer_count = 3; // Thingy and two interchangeable sensor slots
Peer peers[peer_count];
const char* const project_uuid = "a90d7b62-9533-4f8c-aff8-4278bf346914";
std::string assigned[peer_count]; // Accessed only by the scan worker.

std::string sensor_id(const std::string& bytes) {
    if (bytes.size() != 8) return {};
    const char digits[] = "0123456789ABCDEF";
    std::string id;
    for (unsigned char b : bytes) {
        id += digits[b >> 4];
        id += digits[b & 15];
    }
    return id;
}
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
        for (unsigned i = 0; i < peer_count; ++i) {
            if (peers[i].client != client) continue;
            peers[i].parser.reset();
            const char* message = i == 0 ? "BLE:offline" : "Dongle offline";
            publish(peers[i], message, strlen(message));
        }
    }
};
Callbacks callbacks;

void subscribe_peer(unsigned i, NimBLEAdvertisedDevice& device, std::string id) {
    Peer& peer = peers[i];
    if (!peer.client->connect(&device)) return;
    auto* svc = peer.client->getService(nus);
    auto* ch = svc ? svc->getCharacteristic(txchar) : nullptr;
    peer.last.store(millis());
    auto receive = [i, id](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t size, bool) {
        Peer& source = peers[i];
        source.last.store(millis());
        source.parser.feed(bytes, size, [&source, &id](const char* line, size_t len) {
            if (id.empty()) {
                publish(source, line, len);
            } else {
                std::string record = "ID=" + id + " " + std::string(line, len);
                publish(source, record.data(), record.size());
            }
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
                std::string id = sensor_id(device.getServiceData(NimBLEUUID(project_uuid)));
                if (id.empty()) {
                    if (device.getName() == "Thingy53_mLRS" &&
                        device.isAdvertisingService(NimBLEUUID(nus)) &&
                        peers[0].client && !peers[0].client->isConnected())
                        subscribe_peer(0, device, {});
                    continue;
                }
                unsigned slot = peer_count;
                for (unsigned i = 1; i < peer_count; ++i) {
                    if (!peers[i].client) continue;
                    if (peers[i].client->isConnected()) {
                        if (assigned[i] == id) { slot = peer_count; break; }
                    } else if (slot == peer_count) {
                        slot = i;
                    }
                }
                if (slot < peer_count) {
                    assigned[slot] = id;
                    subscribe_peer(slot, device, id);
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
    for (unsigned attempt = 0; attempt < peer_count; ++attempt) {
        unsigned i = (next_source + attempt) % peer_count;
        auto queue = peers[i].latest;
        if (!queue || xQueueReceive(queue, &r, 0) != pdTRUE) continue;
        if ((TickType_t)(xTaskGetTickCount() - r.received) > pdMS_TO_TICKS(15000)) continue;
        memcpy(text, r.text, r.len);
        length = r.len;
        next_source = (i + 1) % peer_count;
        return true;
    }
    return false;
}
void ttgo_ble_flush() {
    for (auto& peer : peers) if (peer.latest) xQueueReset(peer.latest);
}

#endif
