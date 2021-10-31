#pragma once

#include "framework.h"
#include "resource.h"

#include <vector>

#include "BluetoothAudioDevices.h"

class ToothTrayMenu {
private:
public:
    ToothTrayMenu() : m_handle(nullptr) {}

    void BuildMenu(std::vector<BluetoothConnector>& connectors);

    void ShowPopupMenu(HWND hwnd, WPARAM mousPosWParam);

    bool TryHandleCommand(int commandId);
private:
    struct MenuData {
        unsigned int menuId;
        std::wstring menuText;
        BluetoothConnector pConnector;
        MenuData(unsigned int menuId, BluetoothConnector&& pConnector)
            : menuId(menuId), menuText(pConnector.DeviceName()), pConnector(std::move(pConnector)) {}
    };

    wil::unique_hmenu m_handle;
    std::unordered_map<unsigned int, MenuData> m_menuData;

    MENUITEMINFOW InsertBluetoohConnectorMenuItem(UINT id, UINT position, LPWSTR pText);
};