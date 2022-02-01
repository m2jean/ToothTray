#include "BluetoothSocket.h"

void DebugLogSocketResult(INT result, LPCWSTR operation) {
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        DebugLogl(DebugLogStream{} << operation << L" failed: socket_error=" << error);

        if (error == WSA_E_NO_MORE) {
            DebugLog(L"Lookup completed\r\n");
        }
        else if (error == WSAEFAULT) {
            DebugLog(L"Buffer too small\r\n");
        }
        else if (error == WSASERVICE_NOT_FOUND) {
            DebugLog(L"Service not available\r\n");
        }
        else {
            DebugLog(L"Unknown error\r\n");
        }
    }
}

int EnumerateBluetoothDevicesAndServices() {
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (0 != wsaResult) {
        DebugLog(L"Failed to initialize WinSocks2\r\n");
        return wsaResult;
    }

    bool foundCh510 = false;
    BTH_ADDR ch510Addr;
    BTH_QUERY_DEVICE deviceQuery;
    deviceQuery.LAP = 0x9E8B33; // General/Unlimited Inquiry Access Code (GIAC)
    deviceQuery.length = 10; // query for 10 seconds

    BLOB deviceQueryBlob;
    deviceQueryBlob.cbSize = sizeof(BTH_QUERY_DEVICE);
    deviceQueryBlob.pBlobData = reinterpret_cast<BYTE*>(&deviceQuery);

    WSAQUERYSET querySet{ sizeof(WSAQUERYSET) };
    querySet.dwNameSpace = NS_BTH;
    querySet.lpBlob = &deviceQueryBlob;
    HANDLE hLookup;
    INT lookupResult = WSALookupServiceBeginW(&querySet, LUP_CONTAINERS | LUP_FLUSHCACHE, &hLookup);

    if (lookupResult == ERROR_SUCCESS) {
        LookupService lookupService(hLookup);
        while (true) {
            lookupResult = lookupService.LookupNext(LUP_RETURN_TYPE | LUP_RETURN_NAME | LUP_RETURN_ADDR | LUP_RES_SERVICE);
            if (lookupResult != ERROR_SUCCESS)
                break;

            const WSAQUERYSET* queryResult = lookupService.QueryResult();

            bool connected = queryResult->dwOutputFlags & BTHNS_RESULT_DEVICE_CONNECTED;
            bool remembered = queryResult->dwOutputFlags & BTHNS_RESULT_DEVICE_REMEMBERED;
            bool authenticated = queryResult->dwOutputFlags & BTHNS_RESULT_DEVICE_AUTHENTICATED;
            CSADDR_INFO* addresses = queryResult->lpcsaBuffer;

            DebugLogStream dlog;
            dlog << L"Found device: name=" << queryResult->lpszServiceInstanceName << L", class=" << BluetoothDeviceClass(queryResult->lpServiceClassId->Data1);
            for (unsigned int i = 0; i < queryResult->dwNumberOfCsAddrs; ++i) {
                SOCKADDR_BTH* remote = reinterpret_cast<SOCKADDR_BTH*>(addresses[i].RemoteAddr.lpSockaddr);
                dlog << L", address" << i << L'=' << remote->btAddr;
            }

            dlog.Logl();

            if (std::wstring(L"WH-CH510").compare(queryResult->lpszServiceInstanceName) == 0) {
                foundCh510 = true;
                ch510Addr = reinterpret_cast<SOCKADDR_BTH*>(addresses[0].RemoteAddr.lpSockaddr)->btAddr;
            }
        }
    }

    DebugLogSocketResult(lookupResult, L"Device lookup");

    if (hLookup != NULL && WSALookupServiceEnd(hLookup) != ERROR_SUCCESS)
        DebugLog(L"Failed to end device look up\r\n");

    if (foundCh510) {
        EnumerateBluetoothServices(ch510Addr);
    }
}

INT LookupService::LookupNext(DWORD dwControlFlags) {
    DWORD queryResultLength = m_bufferLength;
    INT lookupResult = WSALookupServiceNextW(m_hLookup, dwControlFlags, &queryResultLength, reinterpret_cast<WSAQUERYSET*>(m_buffer.get()));

    if (lookupResult == SOCKET_ERROR && WSAGetLastError() == WSAEFAULT && queryResultLength != 0) {
        m_bufferLength = queryResultLength;
        m_buffer = std::make_unique<BYTE[]>(m_bufferLength);
        lookupResult = WSALookupServiceNextW(m_hLookup, dwControlFlags, &queryResultLength, reinterpret_cast<WSAQUERYSET*>(m_buffer.get()));
    }

    return lookupResult;
}

void EnumerateBluetoothServices(BTH_ADDR address) {
    GUID targetService = PUBLIC_BROWSE_ROOT;
    WCHAR deviceAddress[256];
    DWORD deviceAddressLength = 256;
    SOCKADDR_BTH socketAddress{ AF_BTH, address };
    if (SOCKET_ERROR == WSAAddressToStringW(reinterpret_cast<SOCKADDR*>(&socketAddress), sizeof(SOCKADDR_BTH), NULL, deviceAddress, &deviceAddressLength)) {
        DebugLogl(DebugLogStream{} << L"Failed converting device address to string: error=" << WSAGetLastError());
    }

    WSAQUERYSET serviceQuery{ sizeof(WSAQUERYSET) };
    serviceQuery.dwNameSpace = NS_BTH;
    serviceQuery.lpServiceClassId = &targetService;
    serviceQuery.dwNumberOfCsAddrs = 0;
    serviceQuery.lpszContext = deviceAddress;

    HANDLE hLookup;
    // if LUP_FLUSHCACHE is specified, it only works when the device is online
    INT lookupResult = WSALookupServiceBeginW(&serviceQuery, LUP_FLUSHCACHE, &hLookup);

    if (lookupResult == ERROR_SUCCESS) {
        LookupService lookupService(hLookup);
        while (true) {
            lookupResult = lookupService.LookupNext(LUP_RETURN_TYPE | LUP_RETURN_NAME | LUP_RETURN_ADDR | LUP_RES_SERVICE | LUP_RETURN_BLOB);
            if (lookupResult != ERROR_SUCCESS)
                break;

            const WSAQUERYSET* queryResult = lookupService.QueryResult();

            CSADDR_INFO* addresses = queryResult->lpcsaBuffer;

            DebugLogStream dlog;
            dlog << L"Found service: name=" << queryResult->lpszServiceInstanceName;

            // The simple search returns the service record as the blob
            /* General SDP Service Attributes
            * 0, service record handle
            * 1, service class id list, most specifc to most general
            * 2, service record state
            * 3, service id
            * 4, protocol descriptor list
            * 5, browse group list
            * 6, language based attribute list
            *   +0, service name
            *   +1, service description
            *   +2, provider name
            * 8, service availibity
            * 9, bluetooth profile descriptor list
            * 13, additional protocol descriptor list
            */
            BOOL enumResult = BluetoothSdpEnumAttributes(queryResult->lpBlob->pBlobData, queryResult->lpBlob->cbSize, [](ULONG attrId, BYTE* attrValue, ULONG attrValueLength, VOID* param) -> BOOL {
                DebugLogStream* dlog = reinterpret_cast<DebugLogStream*>(param);
                switch (attrId) {
                case 1: // service class id list
                {
                    *dlog << L", serviceClassIds=[";

                    HBLUETOOTH_CONTAINER_ELEMENT hContainer = NULL;
                    SDP_ELEMENT_DATA value;
                    while (true) {
                        DWORD result = BluetoothSdpGetContainerElementData(attrValue, attrValueLength, &hContainer, &value);
                        if (result != ERROR_SUCCESS) {
                            if (result != ERROR_NO_MORE_ITEMS)
                                *dlog << L"failed to enumerate service class id list";
                            break;
                        }

                        if (value.type != SDP_TYPE_UUID) {
                            *dlog << L"unexpected class id type,";
                            continue;
                        }

                        if (value.specificType == SDP_ST_UUID16)
                            *dlog << std::hex << value.data.uuid16 << std::dec;
                        else if (value.specificType == SDP_ST_UUID32)
                            *dlog << std::hex << value.data.uuid32 << std::dec;
                        else if (value.specificType == SDP_ST_UUID128)
                            *dlog << value.data.uuid128;
                        *dlog << L',';
                    }

                    *dlog << L']';
                    break;
                }
                default:
                    *dlog << L", attrbute=" << attrId;
                }
                return TRUE;
                }, &dlog);
            if (enumResult == FALSE)
                dlog << L", failed to enum attributes";

            dlog.Logl();
        }
    }
}
