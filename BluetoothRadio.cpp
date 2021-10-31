#include "BluetoothRadio.h"
#include "debuglog.h"

#include <string>
#include <initializer_list>
#include <exception>

#include <combaseapi.h>
#include <dbt.h>
#include <Bthsdpdef.h>

constexpr GUID audioSinkService = GUID{ 0x0000110b, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb };

BluetoothRadio BluetoothRadio::FindFirst() {
    BLUETOOTH_FIND_RADIO_PARAMS findParams{ sizeof(findParams) };
    HANDLE hRadio = NULL;
    HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&findParams, &hRadio);
    if (hFind == NULL) {
        DWORD error = GetLastError();
        switch (error) {
        case ERROR_NO_MORE_ITEMS:
            DebugLog(L"No bluetooth radio found\r\n");
            break;
        case ERROR_REVISION_MISMATCH:
            DebugLog(L"Bluetooth find params incorrect size\r\n");
            break;
        default:
            DebugLogl(DebugLogStream{} << L"Bluetooth radio find with unknown error: " << error);
        }
        throw std::exception{};
    }
    if (FALSE == BluetoothFindRadioClose(hFind))
        DebugLog(L"BluetoothFindRadioClose failed\r\n");

    return BluetoothRadio(hRadio);
}

BluetoothRadio::~BluetoothRadio() noexcept {
    if (m_hRadio != NULL && FALSE == CloseHandle(m_hRadio))
        DebugLog(L"CloseHandle on radio handle failed\r\n");
    if (m_hNotify != NULL && FALSE == UnregisterDeviceNotification(m_hNotify))
        DebugLog(L"UnregisterDeviceNotification failed\r\n");
}

void BluetoothRadio::RegisterDeviceChange(HWND hwnd) {
    if (m_hNotify != NULL)
        return;

    DEV_BROADCAST_HANDLE filter{ sizeof(DEV_BROADCAST_HANDLE) };
    filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
    filter.dbch_handle = m_hRadio;
    m_hNotify = RegisterDeviceNotificationW(hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (m_hNotify == NULL) {
        DebugLogl(DebugLogStream{} << L"RegisterDeviceNotificationW with unknown error: " << GetLastError());
        return;
    }
}

const WCHAR* SeparatorWithChange(bool changed) {
    return changed ? L" (changed), " : L", ";
}

void HandleDeviceBroadcast(LPARAM lParam, bool isCustomEvent) {
    const DEV_BROADCAST_HDR* header = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
    switch (header->dbch_devicetype) {
    case DBT_DEVTYP_DEVICEINTERFACE:
    {
        const DEV_BROADCAST_DEVICEINTERFACE_W* deviceInterface = reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE_W*>(lParam);
        DebugLogl(DebugLogStream{} << L"DBT_DEVTYP_DEVICEINTERFACE: name=" << deviceInterface->dbcc_name << L", guid=" << deviceInterface->dbcc_classguid);
    }
    break;
    case DBT_DEVTYP_HANDLE:
    {
        const DEV_BROADCAST_HANDLE* deviceHandle = reinterpret_cast<DEV_BROADCAST_HANDLE*>(lParam);
        DebugLogl(DebugLogStream{} << L"DBT_DEVTYP_DEVICEINTERFACE: handle=" << reinterpret_cast<unsigned long long>(deviceHandle->dbch_handle));

        if (isCustomEvent) {
            if (deviceHandle->dbch_eventguid == GUID_BLUETOOTH_HCI_EVENT) {
                const BTH_HCI_EVENT_INFO* hciInfo = reinterpret_cast<const BTH_HCI_EVENT_INFO*>(deviceHandle->dbch_data);
                DebugLogl(DebugLogStream{} << L"HCI_EVENT : addr=" << hciInfo->bthAddress << L", type=" << hciInfo->connectionType << L", connected=" << hciInfo->connected);
            }
            else if (deviceHandle->dbch_eventguid == GUID_BLUETOOTH_L2CAP_EVENT) {
                const BTH_L2CAP_EVENT_INFO* l2capInfo = reinterpret_cast<const BTH_L2CAP_EVENT_INFO*>(deviceHandle->dbch_data);
                DebugLogl(DebugLogStream{} << L"HCI_EVENT : addr=" << l2capInfo->bthAddress << L", channel=" << l2capInfo->psm << L", connected=" << l2capInfo->connected << L", initiated=" << l2capInfo->initiated);
            }
            else if (deviceHandle->dbch_eventguid == GUID_BLUETOOTH_RADIO_IN_RANGE) {
                const BTH_RADIO_IN_RANGE* radioInRange = reinterpret_cast<const BTH_RADIO_IN_RANGE*>(deviceHandle->dbch_data);
                BTH_DEVICE_INFO deviceInfo = radioInRange->deviceInfo;
                ULONG flags = deviceInfo.flags;

                DebugLogStream dlog;
                dlog << L"RADIO_IN_RANGE: ";
                ULONG changes = flags ^ radioInRange->previousDeviceFlags;
                if (BDIF_ADDRESS & flags) {
                    dlog << L"address=" << deviceInfo.address << SeparatorWithChange(BDIF_ADDRESS & changes);
                }
                else if (BDIF_ADDRESS & changes) {
                    dlog << L"address removed, ";
                }

                if (BDIF_COD & flags) {
                    BTH_COD cod = deviceInfo.classOfDevice;
                    dlog << L"class=" << BluetoothDeviceClass(cod) << SeparatorWithChange(BDIF_COD & changes);
                }
                else if (BDIF_COD & changes) {
                    dlog << L"class removed, ";
                }

                if (BDIF_NAME & flags) {
                    WCHAR buf[BTH_MAX_NAME_SIZE];
                    if (0 == MultiByteToWideChar(CP_UTF8, 0, deviceInfo.name, -1, buf, BTH_MAX_NAME_SIZE))
                        dlog << L"failed to get name, ";
                    else
                        dlog << L"name=" << buf << SeparatorWithChange(BDIF_NAME & changes);
                }
                else if (BDIF_NAME & changes) {
                    dlog << L"name removed, ";
                }

                dlog << L"paired=" << (bool)(BDIF_PAIRED & flags) << SeparatorWithChange(BDIF_PAIRED & changes);
                dlog << L"personal=" << (bool)(BDIF_PERSONAL & flags) << SeparatorWithChange(BDIF_PERSONAL & changes);
                dlog << L"connected=" << (bool)(BDIF_CONNECTED & flags) << SeparatorWithChange(BDIF_CONNECTED & changes);
                dlog << L"support SSP=" << (bool)(BDIF_SSP_SUPPORTED & flags) << SeparatorWithChange(BDIF_SSP_SUPPORTED & changes);
                dlog << L"paired with SSP=" << (bool)(BDIF_SSP_PAIRED & flags) << SeparatorWithChange(BDIF_SSP_PAIRED & changes);
                dlog << L"protected with SSP=" << (bool)(BDIF_SSP_MITM_PROTECTED & flags) << SeparatorWithChange(BDIF_SSP_MITM_PROTECTED & changes);
                dlog.Logl();
            }
            else if (deviceHandle->dbch_eventguid == GUID_BLUETOOTH_RADIO_OUT_OF_RANGE) {
                const BLUETOOTH_ADDRESS* bthAddr = reinterpret_cast<const BLUETOOTH_ADDRESS*>(deviceHandle->dbch_data);
                DebugLogl(DebugLogStream{} << L"RADIO_OUT_OF_RANGE : addr=" << bthAddr->ullLong);
            }
            else {
                DebugLogl(DebugLogStream{} << L"Unknown custom event: guid=" << deviceHandle->dbch_eventguid);
            }
        }
    }
    break;
    case DBT_DEVTYP_OEM:
    {
        const DEV_BROADCAST_OEM* deviceOem = reinterpret_cast<DEV_BROADCAST_OEM*>(lParam);
        DebugLog(L"DBT_DEVTYP_OEM\r\n");
    }
    break;
    case DBT_DEVTYP_PORT:
    {
        const DEV_BROADCAST_PORT_W* devicePort = reinterpret_cast<DEV_BROADCAST_PORT_W*>(lParam);
        DebugLogl(DebugLogStream{} << L"DBT_DEVTYP_PORT: name=" << devicePort->dbcp_name);
    }
    break;
    case DBT_DEVTYP_VOLUME:
    {
        const DEV_BROADCAST_VOLUME* deviceVolume = reinterpret_cast<DEV_BROADCAST_VOLUME*>(lParam);
        DebugLog(L"DBT_DEVTYP_VOLUME\r\n");
    }
    break;
    default:
        break;
    }
}

void HandleDeviceBroadcast(LPARAM lParam) {
    HandleDeviceBroadcast(lParam, false);
}

LRESULT BluetoothRadio::HandleDeviceChangeMessage(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
    case DBT_DEVNODES_CHANGED:
        DebugLog(L"DBT_DEVNODES_CHANGED\r\n");
        break;
    case DBT_QUERYCHANGECONFIG:
        DebugLog(L"DBT_QUERYCHANGECONFIG\r\n");
        break;
    case DBT_CONFIGCHANGED:
        DebugLog(L"DBT_CONFIGCHANGED\r\n");
        break;
    case DBT_CONFIGCHANGECANCELED:
        DebugLog(L"DBT_CONFIGCHANGED\r\n");
        break;
    case DBT_DEVICEARRIVAL:
        DebugLog(L"DBT_DEVICEARRIVAL\r\n");
        HandleDeviceBroadcast(lParam);
        break;
    case DBT_DEVICEQUERYREMOVE:
        DebugLog(L"DBT_DEVICEQUERYREMOVE\r\n");
        HandleDeviceBroadcast(lParam);
        break;
    case DBT_DEVICEQUERYREMOVEFAILED:
        DebugLog(L"DBT_DEVICEQUERYREMOVEFAILED\r\n");
        HandleDeviceBroadcast(lParam);
        break;
    case DBT_DEVICEREMOVEPENDING:
        DebugLog(L"DBT_DEVICEREMOVEPENDING\r\n");
        HandleDeviceBroadcast(lParam);
        break;
    case DBT_DEVICEREMOVECOMPLETE:
        DebugLog(L"DBT_DEVICEREMOVECOMPLETE\r\n");
        HandleDeviceBroadcast(lParam);
        break;
    case DBT_DEVICETYPESPECIFIC:
        DebugLog(L"DBT_DEVICETYPESPECIFIC\r\n");
        HandleDeviceBroadcast(lParam);
        break;
    case DBT_CUSTOMEVENT:
        DebugLog(L"DBT_CUSTOMEVENT");
        HandleDeviceBroadcast(lParam, true);
        break;
    case DBT_USERDEFINED:
    {
        const _DEV_BROADCAST_USERDEFINED* userEvent = reinterpret_cast<_DEV_BROADCAST_USERDEFINED*>(lParam);
        OutputDebugStringA("DBT_USERDEFINED: ");
        OutputDebugStringA(userEvent->dbud_szName);
        OutputDebugStringA("\r\n");
        break;
    }
    default:
        break;
    }

    return TRUE;
}

void BluetoothRadio::FindDevices()
{
    BLUETOOTH_DEVICE_INFO deviceInfo{ sizeof(BLUETOOTH_DEVICE_INFO) };

    // Need 2 different queries for remembered and unknown devices
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams{ sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
    searchParams.hRadio = m_hRadio;
    searchParams.cTimeoutMultiplier = 1; // 12.8s
    searchParams.fReturnAuthenticated = FALSE;
    searchParams.fReturnConnected = FALSE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnUnknown = TRUE;
    searchParams.fIssueInquiry = TRUE;

    HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    BOOL findResult = (hFind != NULL);

    while (findResult == TRUE) {
        DebugLogl(DebugLogStream{} << L"Found bluetooth device: address=" << deviceInfo.Address.ullLong << L", name=" << deviceInfo.szName
            << L", class=" << BluetoothDeviceClass(deviceInfo.ulClassofDevice)
            << L", remembered=" << (bool)deviceInfo.fRemembered << L", connected=" << (bool)deviceInfo.fConnected << L", authenticated=" << (bool)deviceInfo.fAuthenticated
            << L", lastSeen=" << deviceInfo.stLastSeen << L", lastUsed=" << deviceInfo.stLastUsed);

        //if (std::wstring(L"WH-CH510").compare(deviceInfo.szName) == 0) {
        //    m_ch510 = BluetoothDevice(m_hRadio, deviceInfo);
        //}

        findResult = BluetoothFindNextDevice(hFind, &deviceInfo);
    }

    DWORD error = GetLastError();
    if (error == ERROR_NO_MORE_ITEMS)
        DebugLog(L"No more bluetooth devices.\r\n");
    else
        DebugLog(L"Find bluetooth device failed.\r\n");

    if (hFind != NULL)
        BluetoothFindDeviceClose(hFind);
}

void BluetoothRadio::EnableAudioSink() {
    m_ch510.Enable();
}

void BluetoothRadio::DisableAudioSink() {
    m_ch510.Disable();
}

BluetoothDevice::BluetoothDevice(HANDLE hRadio, const BLUETOOTH_DEVICE_INFO& info) : m_hRadio(hRadio), m_info(info), m_services(MAX_SERVICES) {
    DWORD serviceCount = static_cast<DWORD>(m_services.capacity());
    DWORD result = BluetoothEnumerateInstalledServices(m_hRadio, &info, &serviceCount, m_services.data());
    if (result == ERROR_SUCCESS)
        DebugLog(L"Found all services.\r\n");
    else if (result == ERROR_MORE_DATA)
        DebugLog(L"The list of services is incomplete.\r\n");

    m_services.resize(serviceCount);
    for (const GUID& service : m_services) {
        DebugLogl(DebugLogStream{} << L"Found service: " << service);
    }

    /*
    * Need to manually specify the services to enable and disable
    Found service: 00001101-0000-1000-8000-00805f9b34fb serial port
    Found service: 0000110b-0000-1000-8000-00805f9b34fb audio sink
    Found service: 0000110c-0000-1000-8000-00805f9b34fb remote control target
    Found service: 0000110e-0000-1000-8000-00805f9b34fb remote control
    Found service: 0000111e-0000-1000-8000-00805f9b34fb hands free
    */
}

void BluetoothDevice::Enable() {
    for (const GUID& service : m_services) {
        DWORD result = BluetoothSetServiceState(m_hRadio, &m_info, &service, BLUETOOTH_SERVICE_ENABLE);
        if (result == ERROR_SUCCESS) {
            DebugLogl(DebugLogStream{} << L"Enabled service " << service);
        }
        else {
            DebugLogStream dlog;
            dlog << L"Unable to enable service " << service << ": ";
            if (ERROR_INVALID_PARAMETER == result)
                dlog << L"invalid parameters.";
            else if (ERROR_SERVICE_DOES_NOT_EXIST == result)
                dlog << L"service doesn't exist.";
            else if (E_INVALIDARG == result)
                dlog << L"services already enabled.";
            else
                dlog << L"unknown error.";
            dlog.Logl();
        }
    }
}

void BluetoothDevice::Disable () {
    for (const GUID& service : m_services) {
        DWORD result = BluetoothSetServiceState(m_hRadio, &m_info, &service, BLUETOOTH_SERVICE_DISABLE); // throws exception on disabling serial port?
        if (result == ERROR_SUCCESS) {
            DebugLogl(DebugLogStream{} << L"Disabled service " << service);
        }
        else {
            DebugLogStream dlog;
            dlog << L"Unable to disable service " << service << ": ";
            if (ERROR_INVALID_PARAMETER == result)
                dlog << L"invalid parameters.";
            else if (ERROR_SERVICE_DOES_NOT_EXIST == result)
                dlog << L"service doesn't exist.";
            else if (E_INVALIDARG == result)
                dlog << L"services already disabled.";
            else
                dlog << L"unknown error.";
            dlog.Logl();
        }
    }
}
