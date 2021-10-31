#include "framework.h"
#include "BluetoothDeviceClass.h"
#include <vector>
#include <BluetoothAPIs.h>

class BluetoothDevice {
public:
    constexpr BluetoothDevice() : m_hRadio(NULL), m_info({ 0 }) {}
    BluetoothDevice(HANDLE hRadio, const BLUETOOTH_DEVICE_INFO& info);

    void Enable();
    void Disable();
private:
    static constexpr size_t MAX_SERVICES = 10;

    HANDLE m_hRadio;
    BLUETOOTH_DEVICE_INFO m_info;
    std::vector<GUID> m_services;
};

class BluetoothRadio {
public:
    static BluetoothRadio FindFirst();
    constexpr BluetoothRadio(std::nullptr_t) : m_hRadio(NULL), m_hNotify(NULL) {}
    ~BluetoothRadio() noexcept;

    constexpr BluetoothRadio(BluetoothRadio&& other) noexcept : m_hRadio(other.m_hRadio), m_hNotify(other.m_hNotify)
    {
        other.m_hRadio = other.m_hNotify = NULL;
    }

    constexpr BluetoothRadio& operator=(BluetoothRadio&& other) noexcept
    {
        std::swap(m_hRadio, other.m_hRadio);
        std::swap(m_hNotify, other.m_hNotify);
        return *this;
    }

    void RegisterDeviceChange(HWND hwnd);

    static LRESULT HandleDeviceChangeMessage(WPARAM wParam, LPARAM lParam);

    void FindDevices();
    void EnableAudioSink();
    void DisableAudioSink();
private:
    constexpr BluetoothRadio(HANDLE hRadio) : m_hRadio(hRadio), m_hNotify(NULL) {}

    HANDLE m_hRadio;
    HDEVNOTIFY m_hNotify;
    BluetoothDevice m_ch510;
};
