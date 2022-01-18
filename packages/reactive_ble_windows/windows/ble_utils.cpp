// This must be included before many other Windows headers.
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include <map>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "include/reactive_ble_windows/ble_utils.h"


/**
 * @brief Converts winrt::guid to std::string.
 * 
 * @param guid GUID to be converted.
 * @return std::string GUID in string format.
 */
std::string BleUtils::GuidToString(winrt::guid guid)
{
    std::string value = to_string(to_hstring(guid));
    // Strip the leading and trailing curly braces left after the conversion
    if (value.size() > 1)
    {
        value.erase(0, 1);
        value.erase(value.size() - 1);
    }
    return value;
}


/**
 * @brief Converts winrt::guid to std::vector<uint8_t>.
 * 
 * @param guid GUID to be converted.
 * @return std::vector<uint8_t> GUID in byte vector format.
 */
std::vector<uint8_t> BleUtils::GuidToByteVec(winrt::guid guid)
{
    std::vector<uint8_t> result;
    uint8_t* dat1bytes = static_cast<uint8_t*>(static_cast<void*>(&(guid.Data1)));
    uint8_t* dat2bytes = static_cast<uint8_t*>(static_cast<void*>(&(guid.Data2)));
    uint8_t* dat3bytes = static_cast<uint8_t*>(static_cast<void*>(&(guid.Data3)));

    // guid.Data1 = uint32_t
    for (int i = 0; i < 4; i++)
        result.push_back(*(dat1bytes + (3 - i)));

    // guid.Data2 and guid.Data3 = uint16_t
    for (int i = 0; i < 2; i++)
        result.push_back(*(dat2bytes + (1 - i)));
    for (int i = 0; i < 2; i++)
        result.push_back(*(dat3bytes + (1 - i)));

    // guid.Data4 = uint8_t[8] so no reverse order needed
    for (int i = 0; i < 8; i++)
    {
        result.push_back(*(guid.Data4 + i));
    }

    return result;
}

