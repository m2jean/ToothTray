#pragma once
#include <combaseapi.h>
#include <wil/resource.h>
#include <devicetopology.h>
#include <mmdeviceapi.h>

class BluetoothConnector {
public:
    BluetoothConnector(const BluetoothConnector& other) = delete;
    BluetoothConnector& operator=(const BluetoothConnector&) = delete;
    BluetoothConnector(BluetoothConnector&& other) = default;
    BluetoothConnector& operator=(BluetoothConnector&&) = default;

    BluetoothConnector(const std::wstring& containerName)
        : m_deviceName(containerName), m_isConnected(true) {}

    std::wstring_view DeviceName() const {
        return std::wstring_view(m_deviceName);
    }

    void addConnectorControl(const winrt::com_ptr<IKsControl>& connectorControl, DWORD state);

    bool IsConnected() {
        return m_isConnected;
    }

    void Connect() {
        GetKsBtAudioProperty(KSPROPERTY_ONESHOT_RECONNECT);
    }

    void Disconnect() {
        GetKsBtAudioProperty(KSPROPERTY_ONESHOT_DISCONNECT);
    }
private:
    std::wstring m_deviceName;
    bool m_isConnected;
    std::vector<winrt::com_ptr<IKsControl>> m_ksControls;

    void GetKsBtAudioProperty(ULONG property);
};

class BluetoothAudioDeviceEnumerator {
public:
    std::vector<BluetoothConnector> EnumerateAudioDevices();
    std::vector<BluetoothConnector>::iterator ConnectorsBegin() {
        return m_bluetoothConnectors.begin();
    }
    std::vector<BluetoothConnector>::iterator ConnectorsEnd() {
        return m_bluetoothConnectors.end();
    }
private:
    std::vector<BluetoothConnector> m_bluetoothConnectors;
};
