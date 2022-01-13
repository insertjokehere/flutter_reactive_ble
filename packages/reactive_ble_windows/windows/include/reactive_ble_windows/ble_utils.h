#ifndef BLE_WIN_UTILS_H
#define BLE_WIN_UTILS_H

// This must be included before many other Windows headers.
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

namespace BleUtils {
    std::string to_uuidstr(winrt::guid);
}

#endif