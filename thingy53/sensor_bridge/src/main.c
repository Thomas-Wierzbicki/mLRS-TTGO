/* Thingy:53 environmental measurements over Nordic UART Service.
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include <bluetooth/services/nus.h>
#include <dk_buttons_and_leds.h>
#include <errno.h>
#include <string.h>
#include "sensor_text.h"

LOG_MODULE_REGISTER(thingy_bridge, LOG_LEVEL_INF);
static const struct device *env = DEVICE_DT_GET(DT_NODELABEL(bme688));
static struct bt_conn *peer;
K_MUTEX_DEFINE(peer_lock);
static atomic_t subscribed;
static struct k_work_delayable adv_work;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(struct k_work *work)
{
    ARG_UNUSED(work);
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad),
                             sd, ARRAY_SIZE(sd));
    if (err && err != -EALREADY) {
        LOG_WRN("Advertising: %d; retry", err);
        k_work_reschedule(&adv_work, K_SECONDS(1));
    }
}
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_WRN("Connect failed: %u", err);
        k_work_reschedule(&adv_work, K_SECONDS(1));
        return;
    }
    k_mutex_lock(&peer_lock, K_FOREVER);
    peer = bt_conn_ref(conn);
    atomic_clear(&subscribed);
    k_mutex_unlock(&peer_lock);
    dk_set_led_on(DK_LED2);
    LOG_INF("BLE connected; enable NUS TX notifications");
}
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    struct bt_conn *old = NULL;
    k_mutex_lock(&peer_lock, K_FOREVER);
    if (peer == conn) {
        old = peer;
        peer = NULL;
        atomic_clear(&subscribed);
    }
    k_mutex_unlock(&peer_lock);
    if (old) { bt_conn_unref(old); }
    dk_set_led_off(DK_LED2);
    LOG_INF("BLE disconnected: %u", reason);
}
static void recycled(void)
{
    /* A fresh connection slot is available only after the final unref. */
    k_work_reschedule(&adv_work, K_NO_WAIT);
}
BT_CONN_CB_DEFINE(callbacks) = {
    .connected = connected, .disconnected = disconnected, .recycled = recycled,
};
static void send_enabled(enum bt_nus_send_status status)
{
    atomic_set(&subscribed, status == BT_NUS_SEND_STATUS_ENABLED);
}
static struct bt_nus_cb nus_callbacks = { .send_enabled = send_enabled };

static void send_line(const char *text)
{
    struct bt_conn *conn = NULL;
    k_mutex_lock(&peer_lock, K_FOREVER);
    if (peer && atomic_get(&subscribed)) { conn = bt_conn_ref(peer); }
    k_mutex_unlock(&peer_lock);
    if (!conn) { return; }

    /* 20-byte pieces also work with the default ATT MTU of 23.
     * Each record starts AND ends with LF, so a partial previous record
     * cannot be joined to the next record after a failed send.
     */
    size_t length = strlen(text);
    for (size_t offset = 0; offset < length;) {
        uint16_t count = (uint16_t)MIN(length - offset, 20U);
        int err = bt_nus_send(conn, (const uint8_t *)text + offset, count);
        if (err) {
            LOG_WRN("NUS send: %d; discard record", err);
            break;
        }
        offset += count;
        k_sleep(K_MSEC(10));
    }
    bt_conn_unref(conn);
}

static int64_t micro_value(const struct sensor_value *v)
{
    return (int64_t)v->val1 * 1000000 + v->val2;
}
int main(void)
{
    int err = dk_leds_init();
    if (err) { LOG_WRN("LED init: %d", err); }
    k_work_init_delayable(&adv_work, advertise);
    err = bt_enable(NULL);
    if (err) { LOG_ERR("Bluetooth init: %d", err); return 0; }
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        err = settings_load();
        if (err) { LOG_WRN("Settings: %d", err); }
    }
    err = bt_nus_init(&nus_callbacks);
    if (err) { LOG_ERR("NUS init: %d", err); return 0; }
    k_work_reschedule(&adv_work, K_NO_WAIT);
    LOG_INF("Thingy53_mLRS sensor bridge started");

    for (;;) {
        int64_t next_sample = k_uptime_get() + 5000;
        struct sensor_value t, h, p;
        char line[80];
        err = device_is_ready(env) ? sensor_sample_fetch(env) : -ENODEV;
        if (!err) { err = sensor_channel_get(env, SENSOR_CHAN_AMBIENT_TEMP, &t); }
        if (!err) { err = sensor_channel_get(env, SENSOR_CHAN_HUMIDITY, &h); }
        if (!err) { err = sensor_channel_get(env, SENSOR_CHAN_PRESS, &p); }
        if (err) {
            dk_set_led_on(DK_LED1);
            snprintf(line, sizeof(line), "\nERR:BME688:%d\n", err);
            LOG_ERR("BME688 read: %d", err);
        } else {
            dk_set_led_off(DK_LED1);
            /* Zephyr pressure is kPa; text output is absolute hPa. */
            int n = sensor_text(line, sizeof(line), micro_value(&t),
                                micro_value(&h), micro_value(&p));
            if (n < 0 || (size_t)n >= sizeof(line)) {
                LOG_ERR("Text buffer overflow");
                k_sleep(K_SECONDS(5));
                continue;
            }
            LOG_INF("%s", line + 1);
        }
        send_line(line);
        dk_set_led_on(DK_LED3);
        k_sleep(K_MSEC(50));
        dk_set_led_off(DK_LED3);
        int64_t wait_ms = next_sample - k_uptime_get();
        if (wait_ms > 0) { k_sleep(K_MSEC(wait_ms)); }
    }
}
