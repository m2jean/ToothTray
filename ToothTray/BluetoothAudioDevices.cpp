#include <Unknwn.h>
#include "framework.h"
#include "BluetoothAudioDevices.h"

#include <combaseapi.h>
#include <wil/resource.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include "DeviceContainerEnumerator.h"
#include "debuglog.h"

std::wstring GetDeviceName(IPropertyStore& propertyStore) {
    wil::unique_prop_variant propName;
    PropVariantInit(&propName);
    propertyStore.GetValue(PKEY_Device_FriendlyName, &propName);
    LPCWSTR name = propName.vt == VT_EMPTY ? L"no name" : propName.pwszVal;
    return std::wstring(name);
}

GUID GetContainerId(IPropertyStore& propertyStore) {
    wil::unique_prop_variant propContainerId;
    PropVariantInit(&propContainerId);
    propertyStore.GetValue(PKEY_Device_ContainerId, &propContainerId);
    GUID containerId = propContainerId.vt == VT_EMPTY ? GUID_NULL : *propContainerId.puuid;
    return containerId;
}

std::vector<BluetoothConnector> BluetoothAudioDeviceEnumerator::EnumerateAudioDevices() {
    std::unordered_map<GUID, BluetoothConnector, GUIDHasher, GUIDEqualityComparer> bluetoothConnectors;
    std::unordered_map<GUID, std::wstring, GUIDHasher, GUIDEqualityComparer> containers = DeviceContainerEnumerator::EnumerateContainers();

    wil::com_ptr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), pEnumerator.put_void());
    DebugLogHresult(hr);

    wil::com_ptr <IMMDeviceCollection> pDevices;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATEMASK_ALL, pDevices.put());
    DebugLogHresult(hr);

    UINT deviceCount = 0;
    hr = pDevices->GetCount(&deviceCount);

    for (UINT i = 0; i < deviceCount; ++i) {
        wil::com_ptr<IMMDevice> pDevice;
        pDevices->Item(i, pDevice.put());

        // No need to get IMMEndpoint because we already know it's a render device
        //winrt::com_ptr<IMMEndpoint> pEndPoint = pDevice.as<IMMEndpoint>();

        wil::unique_cotaskmem_string pDeviceId;
        pDevice->GetId(pDeviceId.put());

        DWORD state;
        pDevice->GetState(&state);
        LPCWSTR stateStr;
        switch (state) {
        case DEVICE_STATE_ACTIVE:
            stateStr = L"active";
            break;
        case DEVICE_STATE_DISABLED:
            stateStr = L"disabled";
            break;
        case DEVICE_STATE_NOTPRESENT:
            stateStr = L"absent";
            break;
        case DEVICE_STATE_UNPLUGGED:
            stateStr = L"unplugged";
            break;
        }


        wil::com_ptr<IPropertyStore> pPropertyStore;
        pDevice->OpenPropertyStore(STGM_READ, pPropertyStore.put());
        std::wstring deviceName = GetDeviceName(*pPropertyStore.get());
        GUID containerId = GetContainerId(*pPropertyStore.get());

        DebugLogl(DebugLogStream{} << L"device name: " << deviceName << L", state: " << stateStr << ", id: " << pDeviceId.get() << ", container: " << containerId);

        wil::com_ptr<IDeviceTopology> pTopology;
        pDevice->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, pTopology.put_void());

        UINT connectorCount;
        pTopology->GetConnectorCount(&connectorCount);
        for (UINT i = 0; i < connectorCount; ++i) {
            wil::com_ptr<IConnector> pConnector;
            pTopology->GetConnector(i, pConnector.put());

            wil::com_ptr<IConnector> pOtherConnector;
            pConnector->GetConnectedTo(pOtherConnector.put());
            if (pOtherConnector == nullptr)
                continue;

            wil::com_ptr<IPart> pPart{ pOtherConnector.query<IPart>() };
            wil::com_ptr<IDeviceTopology> pOtherTopology;
            pPart->GetTopologyObject(pOtherTopology.put());
            wil::unique_cotaskmem_string otherDeviceId;
            pOtherTopology->GetDeviceId(otherDeviceId.put());

            DebugLogl(DebugLogStream{} << L"connected to " << otherDeviceId.get());

            if (!std::wstring_view(otherDeviceId.get()).starts_with(LR""({2}.\\?\bth)"")) // bthenum or bthhfenum
                continue;

            wil::com_ptr<IMMDevice> pOtherDevice;
            pEnumerator->GetDevice(otherDeviceId.get(), pOtherDevice.put());

            wil::com_ptr<IKsControl> pKsControl;
            pOtherDevice->Activate(__uuidof(IKsControl), CLSCTX_ALL, NULL, pKsControl.put_void());

            std::unordered_map<GUID, BluetoothConnector, GUIDHasher, GUIDEqualityComparer>::iterator ite = bluetoothConnectors.find(containerId);
            if (ite == bluetoothConnectors.end()) {
                std::unordered_map<GUID, std::wstring, GUIDHasher, GUIDEqualityComparer>::const_iterator containerIte = containers.find(containerId);
                if (containerIte != containers.cend()) {
                    const std::wstring& containerName = containerIte->second;
                    ite = bluetoothConnectors.emplace(std::piecewise_construct, std::forward_as_tuple(containerId), std::forward_as_tuple(containerName)).first;
                }
            }

            ite->second.addConnectorControl(pKsControl, state);
        }
    }
    
    std::vector<BluetoothConnector> connectors;
    for (auto ite = bluetoothConnectors.begin(); ite != bluetoothConnectors.end(); ++ite)
        connectors.emplace_back(std::move(ite->second));
    return connectors;
}

void BluetoothConnector::addConnectorControl(const wil::com_ptr<IKsControl>& connectorControl, DWORD state) {
    m_ksControls.emplace_back(connectorControl);
    m_isConnected |= state == DEVICE_STATE_ACTIVE;
}

void BluetoothConnector::GetKsBtAudioProperty(ULONG property) {
    KSPROPERTY ksProperty;
    ksProperty.Set = KSPROPSETID_BtAudio;
    ksProperty.Id = property;
    ksProperty.Flags = KSPROPERTY_TYPE_GET;

    ULONG bytesReturned;
    for (wil::com_ptr<IKsControl> &ksControl : m_ksControls) {
        HRESULT hr = ksControl->KsProperty(&ksProperty, sizeof(ksProperty), NULL, 0, &bytesReturned);
        DebugLogHresult(hr);
    }
}
