#include "BluetoothDeviceClass.h"

std::wostream& operator<<(std::wostream& stream, BluetoothDeviceClass cod) {
    stream << GET_COD_MAJOR(cod.cod) << L'.' << GET_COD_MINOR(cod.cod) << L"." << GET_COD_SERVICE(cod.cod);
    return stream;
}