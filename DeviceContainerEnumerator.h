#pragma once

#include <winrt\Windows.Foundation.Collections.h>
#include <winrt\Windows.Devices.Enumeration.h>
#include <unordered_map>

#include "debuglog.h"

struct GUIDEqualityComparer {
	bool operator()(const GUID& guid1, const GUID& guid2) const {
		return std::memcmp(&guid1, &guid2, sizeof(GUID)) == 0;
	}
};

struct GUIDHasher {
	UINT operator()(const GUID& guid) const {
		return guid.Data1 ^ ((guid.Data2 << 16) | guid.Data3) ^ (*(UINT*)guid.Data4) ^ (*(UINT*)(guid.Data4 + 4));
	}
};

class DeviceContainerEnumerator {
public:
	static std::unordered_map<GUID, std::wstring, GUIDHasher, GUIDEqualityComparer> EnumerateContainers();
};