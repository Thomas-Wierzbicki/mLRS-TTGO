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
QueueHandle_t latest = nullptr;
TaskHandle_t worker = nullptr;
TtgoBleLine parser; // Only used by NimBLE host callbacks.
std::atomic<uint32_t> lastNotification{0};

void publish(const char* text, size_t length) {
    Record r = {};
    if (!latest || !length || length > 95) return;
    // Leading newline recovers receiver framing after a radio interruption.
    r.text[0] = '\n';
    memcpy(r.text + 1, text, length);
    r.text[length + 1] = '\n';
    r.len = length + 2;
    r.received = xTaskGetTickCount();
    xQueueOverwrite(latest, &r); // Never accumulate stale sensor history.
}
void notify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    lastNotification.store(millis(), std::memory_order_relaxed);
    parser.feed(data, len, [](const char* s, size_t n) { publish(s, n); });
}
class Callbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*) override {
        parser.reset();
        publish("BLE:offline", 11);
    }
};
Callbacks callbacks;

void run(void*) {
    NimBLEDevice::init("TTGO_mLRS_TX");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true); // Name is in Thingy's scan response.
    scan->setInterval(160);
    scan->setWindow(40);
    NimBLEClient* client = NimBLEDevice::createClient();
    if (!client) {
        publish("BLE:init error", 14);
        vTaskDelete(nullptr);
        return;
    }
    client->setClientCallbacks(&callbacks, false);
    client->setConnectTimeout(5);
    client->setConnectionParams(24, 40, 0, 400);
    publish("BLE:searching", 13);
    for (;;) {
        if (!client->isConnected()) {
            NimBLEScanResults results = scan->start(3, false);
            for (int i = 0; i < results.getCount(); ++i) {
                NimBLEAdvertisedDevice peer = results.getDevice(i);
                if (peer.getName() != "Thingy53_mLRS" ||
                    !peer.isAdvertisingService(NimBLEUUID(nus))) continue;
                if (!client->connect(&peer)) break;
                // Rediscover services on every connection (firmware may change).
                NimBLERemoteService* service = client->getService(nus);
                NimBLERemoteCharacteristic* ch = service ? service->getCharacteristic(txchar) : nullptr;
                lastNotification.store(millis(), std::memory_order_relaxed);
                if (!ch || !ch->canNotify() || !ch->subscribe(true, notify, true)) {
                    client->disconnect();
                }
                break;
            }
            scan->clearResults();
        } else if ((uint32_t)(millis() - lastNotification.load(std::memory_order_relaxed)) > 20000) {
            // Recover if a connection survives but notifications stop.
            client->disconnect();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}

void ttgo_ble_init() {
    if (worker) return; // mLRS can reinitialize its configuration without reboot.
    if (!latest) latest = xQueueCreate(1, sizeof(Record));
    if (!latest) return;
    if (xTaskCreatePinnedToCore(run, "thingy_ble", 6144, nullptr, 1, &worker, 0) != pdPASS) {
        worker = nullptr;
        publish("BLE:task error", 14);
    }
}
bool ttgo_ble_take(char* text, uint16_t& length) {
    Record r = {};
    length = 0;
    if (!latest || xQueueReceive(latest, &r, 0) != pdTRUE) return false;
    if ((TickType_t)(xTaskGetTickCount() - r.received) > pdMS_TO_TICKS(15000)) return false;
    memcpy(text, r.text, r.len);
    length = r.len;
    return length != 0;
}
void ttgo_ble_flush() {
    if (latest) xQueueReset(latest);
}

#endif
