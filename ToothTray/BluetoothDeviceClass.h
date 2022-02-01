#pragma once

#include "framework.h"
#include <BluetoothAPIs.h>

struct BluetoothDeviceClass {
    BTH_COD cod;
    BluetoothDeviceClass(BTH_COD cod) : cod(cod) {}
};

std::wostream& operator<<(std::wostream& stream, BluetoothDeviceClass cod);
