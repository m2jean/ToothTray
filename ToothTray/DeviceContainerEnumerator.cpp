#include "DeviceContainerEnumerator.h"

#include <combaseapi.h>

static GUID from_id(winrt::hstring id) {
	GUID guid;
	HRESULT hr = CLSIDFromString(id.c_str(), &guid);
	DebugLogHresult(hr);
	return guid;
}

std::unordered_map<GUID, std::wstring, GUIDHasher, GUIDEqualityComparer> DeviceContainerEnumerator::EnumerateContainers() {
	std::unordered_map<GUID, std::wstring, GUIDHasher, GUIDEqualityComparer> containers;

	winrt::Windows::Devices::Enumeration::DeviceInformationCollection collection =
		winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(winrt::hstring(), nullptr, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceContainer).get();

	for (winrt::Windows::Devices::Enumeration::DeviceInformation info : collection) {
		containers.insert_or_assign(from_id(info.Id()), std::wstring(info.Name().c_str()));
	}

	return containers;
}
