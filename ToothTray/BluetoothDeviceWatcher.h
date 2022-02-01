#pragma once

#include "framework.h"
#include <winrt\Windows.Foundation.h>
#include <winrt\Windows.Devices.Bluetooth.h>
#include <unordered_map>

class DeviceInfo {
public:
    winrt::hstring name;
    bool canPair;
    bool isPaired;

    DeviceInfo(winrt::hstring name, bool canPair, bool isPaired)
        : name(name), canPair(canPair), isPaired(isPaired) {}
};

class BluetoothDeviceWatcher {
public:
    BluetoothDeviceWatcher();

    void Start();
private:
    winrt::Windows::Devices::Enumeration::DeviceWatcher m_watcher{ nullptr };
    winrt::Windows::Devices::Enumeration::DeviceWatcher::EnumerationCompleted_revoker m_devicedEnumerationCompletedRevoker;
    winrt::Windows::Devices::Enumeration::DeviceWatcher::Added_revoker m_devicedAddedRevoker;
    winrt::Windows::Devices::Enumeration::DeviceWatcher::Updated_revoker m_devicedUpdatedRevoker;
    winrt::Windows::Devices::Enumeration::DeviceWatcher::Removed_revoker m_deviceRemovedRevoker;

    std::unordered_map<winrt::hstring, DeviceInfo> m_devices;

    void DeviceAdded(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Devices::Enumeration::DeviceInformation info);
    void DeviceUpdated(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Devices::Enumeration::DeviceInformationUpdate update);
    void DeviceRemoved(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Devices::Enumeration::DeviceInformationUpdate update);
};

