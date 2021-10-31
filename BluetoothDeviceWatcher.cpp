#include "BluetoothDeviceWatcher.h"

#include <winrt\Windows.Foundation.Collections.h>
#include <winrt\Windows.Devices.Enumeration.h>
#include <debugapi.h>
#include <sstream>

void DeviceEnumerationCompleted(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Foundation::IInspectable _) {
    UNREFERENCED_PARAMETER(watcher);
    OutputDebugStringW(L"Device enumeration completed.\r\n");
}

void OutputDeviceProperties(std::wostringstream& sout, const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Foundation::IInspectable>& properties) {
    for (const auto& kvp : properties) {
        const winrt::hstring&& propName = kvp.Key();
        if (propName == L"System.Devices.Aep.ContainerId") {
            winrt::guid containerId = kvp.Value().as<winrt::guid>();
            sout << propName.c_str();
        }
        else {
            sout << propName.c_str() << ", ";
        }
    }
    sout << std::endl;
}

BluetoothDeviceWatcher::BluetoothDeviceWatcher() {
    winrt::hstring pairedSelector = winrt::Windows::Devices::Bluetooth::BluetoothDevice::GetDeviceSelectorFromPairingState(true);
    m_watcher = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(pairedSelector, nullptr, winrt::Windows::Devices::Enumeration::DeviceInformationKind::AssociationEndpoint);

    m_devicedEnumerationCompletedRevoker = m_watcher.EnumerationCompleted(winrt::auto_revoke, DeviceEnumerationCompleted);
    m_devicedAddedRevoker = m_watcher.Added(winrt::auto_revoke, { this, &BluetoothDeviceWatcher::DeviceAdded });
    m_devicedUpdatedRevoker = m_watcher.Updated(winrt::auto_revoke, { this, &BluetoothDeviceWatcher::DeviceUpdated });
    m_deviceRemovedRevoker = m_watcher.Removed(winrt::auto_revoke, { this, &BluetoothDeviceWatcher::DeviceRemoved });
}

void BluetoothDeviceWatcher::Start()
{
    m_watcher.Start();
}

void BluetoothDeviceWatcher::DeviceAdded(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Devices::Enumeration::DeviceInformation info) {
    UNREFERENCED_PARAMETER(watcher);

    winrt::hstring&& name = info.Name();
    winrt::hstring&& id = info.Id();
    winrt::Windows::Devices::Enumeration::DeviceInformationKind kind = info.Kind();
    bool isEnabled = info.IsEnabled();
    winrt::Windows::Devices::Enumeration::DeviceInformationPairing&& pairing = info.Pairing();
    bool canPair = pairing.CanPair();
    bool isPaired = pairing.IsPaired();

    std::wostringstream sout;
    sout << L"Device added: ";
    sout << L"name: " << name.c_str() << ", ";
    sout << L"id: " << id.c_str() << ", ";
    sout << L"kind: " << static_cast<int>(kind) << ", ";
    sout << L"is enabled: " << isEnabled << ", ";
    sout << L"can pair: " << canPair << ", ";
    sout << L"is paired: " << isPaired << ", ";

    OutputDeviceProperties(sout, info.Properties());

    const std::wstring&& output = sout.str();
    OutputDebugStringW(output.c_str());

    m_devices.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(name, canPair, isPaired));

    winrt::Windows::Devices::Bluetooth::BluetoothDevice device = winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromIdAsync(id).get();
        //.Completed(
        //[](winrt::Windows::Devices::Bluetooth::BluetoothDevice device) {
            OutputDebugStringW(L"found bluetooth device\r\n");
        //}
    //);
}

void BluetoothDeviceWatcher::DeviceUpdated(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Devices::Enumeration::DeviceInformationUpdate update) {
    UNREFERENCED_PARAMETER(watcher);

    winrt::hstring&& id = update.Id();
    winrt::Windows::Devices::Enumeration::DeviceInformationKind kind = update.Kind();

    std::wostringstream sout;
    sout << L"Device updated: ";
    sout << L"id: " << id.c_str() << L", ";
    sout << L"kind: " << static_cast<int>(kind) << L", ";

    const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Foundation::IInspectable>&& properties = update.Properties();
    OutputDeviceProperties(sout, properties);

    OutputDebugStringW(sout.str().c_str());

    sout.str(L"");
    DeviceInfo& info = m_devices.at(id);
    sout << info.name.c_str() << L" updated: ";
    for (const auto& kvp : properties) {
        const winrt::hstring&& propName = kvp.Key();
        winrt::Windows::Foundation::IInspectable&& value = kvp.Value();

        if (propName == L"System.ItemNameDisplay") {
            winrt::hstring name = winrt::unbox_value<winrt::hstring>(value);
            if (info.name == name)
                continue;
            sout << L"name: " << info.name.c_str() << L" -> " << name.c_str() << L"; ";
            info.name = name;
        }
        else if (propName == L"System.Devices.Aep.CanPair") {
            bool canPair = winrt::unbox_value<bool>(value);
            if (info.canPair == canPair)
                continue;
            sout << L"can pair: " << info.canPair << L" -> " << canPair << L"; ";
            info.canPair = canPair;
        }
        else if (propName == L"System.Devices.Aep.IsPaired") {
            bool isPaired = winrt::unbox_value<bool>(value);
            if (info.isPaired == isPaired)
                continue;
            sout << L"is paired: " << info.isPaired << L" -> " << isPaired << L"; ";
            info.isPaired = isPaired;
        }
    }
    sout << std::endl;
    OutputDebugStringW(sout.str().c_str());
}

void BluetoothDeviceWatcher::DeviceRemoved(winrt::Windows::Devices::Enumeration::DeviceWatcher watcher, winrt::Windows::Devices::Enumeration::DeviceInformationUpdate update) {
    UNREFERENCED_PARAMETER(watcher);

    winrt::hstring&& id = update.Id();

    std::wostringstream sout;
    sout << L"Device removed: ";
    sout << L"id: " << id.c_str() << ", ";

    const DeviceInfo& info = m_devices.at(id);
    sout << L"name: " << info.name.c_str() << ", ";

    OutputDeviceProperties(sout, update.Properties());

    const std::wstring&& output = sout.str();
    OutputDebugStringW(output.c_str());
}
