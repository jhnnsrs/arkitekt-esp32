#ifndef RUN_CONFIG_H
#define RUN_CONFIG_H

#include <Arduino.h>

struct RunConfig
{
    bool ble = true;
    bool enableWpa2Enterprise = true;
    unsigned long bootReconfigureTimeout = 5000;
    const char *bleDeviceName = "ARKITEKT_CONFIG";
    String redeemToken = "";
    String baseUrl = "";
};

#endif // RUN_CONFIG_H
