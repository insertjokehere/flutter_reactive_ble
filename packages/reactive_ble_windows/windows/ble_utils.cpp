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
        result.push_back(*(guid.Data4 + i));

    return result;
}


/**
 * @brief Converts a Uuid (defined by bledata.proto) into a formatted string.
 * 
 * @param uuid The Uuid to convert into a string.
 * @return std::string The string equivalent of the given Uuid, with hyphens.
 */
std::string BleUtils::ProtobufUuidToString(Uuid uuid)
{
    unsigned char* x = (unsigned char*) uuid.data().c_str();
    std::stringstream ss;
    ss << std::hex;
    for (size_t i = 0; i < uuid.data().length(); i++)
    {
        ss << std::setw(2) << std::setfill('0') << (int)x[i];
    }
    std::string result = ss.str();

    // UUID format: 01234567-8901-2345-6789-012345678901
    //                       ^    ^    ^    ^
    // Index:                8    12   16   20
    result.insert(20, "-");
    result.insert(16, "-");
    result.insert(12, "-");
    result.insert(8, "-");
    return result;
}


/**
 * @brief Converts a Uuid (defined by bledata.proto) into a winrt::guid.
 * 
 * @param uuid The Uuid to convert into a guid.
 * @return winrt::guid The guid equivalent of the given Uuid.
 */
winrt::guid BleUtils::ProtobufUuidToGuid(Uuid uuid)
{
    winrt::guid result;
    unsigned char* rawData = (unsigned char*) uuid.data().c_str();
    result.Data1 = (rawData[0] << 24) | (rawData[1] << 16) | (rawData[2] << 8) | rawData[3];
    result.Data2 = (rawData[4] << 8) | rawData[5];
    result.Data3 = (rawData[6] << 8) | rawData[7];

    for (int i = 0; i < 8; i++)
        result.Data4[i] = rawData[8 + i];

    return result;
}
