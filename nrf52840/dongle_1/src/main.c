// SPDX-License-Identifier: MIT
#include <errno.h>
#include <nrfx.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/printk.h>
#include <bluetooth/services/nus.h>

static uint8_t project_data[24] = {
    BT_UUID_128_ENCODE(0xa90d7b62, 0x9533, 0x4f8c, 0xaff8, 0x4278bf346914)
};
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_SVC_DATA128, project_data, sizeof(project_data)),
};
static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
static void advertise(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(adv_work, advertise);
static void advertise(struct k_work *work)
{
    ARG_UNUSED(work);
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err && err != -EALREADY) k_work_reschedule(&adv_work, K_SECONDS(1));
}
static void recycled(void) { k_work_reschedule(&adv_work, K_NO_WAIT); }
BT_CONN_CB_DEFINE(callbacks) = { .recycled = recycled };

int main(void)
{
    const struct device *temp = DEVICE_DT_GET(DT_NODELABEL(temp));
    sys_put_be32(NRF_FICR->DEVICEID[1], project_data + 16);
    sys_put_be32(NRF_FICR->DEVICEID[0], project_data + 20);
    if (bt_enable(NULL) || bt_nus_init(NULL)) return 1;
    k_work_reschedule(&adv_work, K_NO_WAIT);
    for (;;) {
        struct sensor_value v;
        char line[21]; // A complete line fits a single default-MTU notification.
        int n;
        int err = device_is_ready(temp) ? sensor_sample_fetch(temp) : -ENODEV;
        if (!err) err = sensor_channel_get(temp, SENSOR_CHAN_DIE_TEMP, &v);
        if (err) n = snprintk(line, sizeof(line), "\nTEMP ERR\n");
        else {
            int centi = v.val1 * 100 + v.val2 / 10000;
            int magnitude = centi < 0 ? -centi : centi;
            n = snprintk(line, sizeof(line), "\nChip %s%d.%02dC\n",
                         centi < 0 ? "-" : "", magnitude / 100, magnitude % 100);
        }
        if (n > 0 && n <= 20) bt_nus_send(NULL, (uint8_t *)line, n);
        k_sleep(K_SECONDS(5));
    }
}
