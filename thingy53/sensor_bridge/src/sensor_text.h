/* SPDX-License-Identifier: MIT */
#ifndef SENSOR_TEXT_H
#define SENSOR_TEXT_H
#include <stdint.h>
#include <stdio.h>
/* Inputs: micro-degrees C, micro-percent RH, micro-kPa. */
static inline int sensor_text(char *out, size_t size, int64_t t, int64_t h, int64_t p)
{
    /* Round temperature to tenths, preserving -0.x. */
    int64_t temp10 = t >= 0 ? (t + 50000) / 100000 : -((-t + 50000) / 100000);
    int64_t abs10 = temp10 < 0 ? -temp10 : temp10;
    long rh = (long)((h + 500000) / 1000000);
    long hpa = (long)((p + 50000) / 100000);
    return snprintf(out, size, "\nT:%s%ld.%ldC H:%ld%% P:%ldhPa\n",
                    temp10 < 0 ? "-" : "", (long)(abs10 / 10),
                    (long)(abs10 % 10), rh, hpa);
}
#endif
