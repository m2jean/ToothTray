#pragma once
#include "framework.h"

#include <winsock2.h>
#include <ws2bth.h>
#include <bluetoothapis.h>

#include "debuglog.h"
#include "BluetoothDeviceClass.h"

void DebugLogSocketResult(INT result, LPCWSTR operation);

constexpr GUID SERVICE_BASE = GUID{ 0x00000000, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb };
constexpr GUID PUBLIC_BROWSE_ROOT = GUID{ 0x00001002, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb };

class LookupService {
public:
    LookupService(HANDLE hLookup) : m_hLookup(hLookup), m_bufferLength(sizeof(WSAQUERYSET)), m_buffer(std::make_unique<BYTE[]>(m_bufferLength)) {}

    INT LookupNext(DWORD dwControlFlags);

    const WSAQUERYSET* QueryResult() { return reinterpret_cast<WSAQUERYSET*>(m_buffer.get()); }
private:
    HANDLE m_hLookup;
    DWORD m_bufferLength;
    std::unique_ptr<BYTE[]> m_buffer;
};

int EnumerateBluetoothDevicesAndServices();

void EnumerateBluetoothServices(BTH_ADDR address);
