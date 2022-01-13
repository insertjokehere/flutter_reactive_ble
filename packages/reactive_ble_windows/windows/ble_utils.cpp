// This must be included before many other Windows headers.
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include <map>
#include <algorithm>

#include "include/reactive_ble_windows/ble_utils.h"


#define GUID_FORMAT "%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define GUID_ARG(guid) guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]


/**
 * @brief Converts winrt::guid to std::string.
 * 
 * @param guid GUID to be converted.
 * @return std::string GUID in string format, according to GUID_FORMAT and GUID_ARG structure.
 */
std::string BleUtils::to_uuidstr(winrt::guid guid)
{
    char chars[36 + 1];
    sprintf_s(chars, GUID_FORMAT, GUID_ARG(guid));
    return std::string{chars};
}

