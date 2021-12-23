// This must be included before many other Windows headers.
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include <flutter/method_channel.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/standard_method_codec.h>
#include <flutter/standard_message_codec.h>

#include <map>
#include <algorithm>

namespace
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Storage::Streams;
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    using flutter::EncodableMap;
    using flutter::EncodableValue;

    #define GUID_FORMAT "%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
    #define GUID_ARG(guid) guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]


    std::string to_uuidstr(winrt::guid guid)
    {
        char chars[36 + 1];
        sprintf_s(chars, GUID_FORMAT, GUID_ARG(guid));
        return std::string{chars};
    }


    struct BluetoothDeviceAgent
    {
        BluetoothLEDevice device;
        winrt::event_token connnectionStatusChangedToken;
        std::map<std::string, GattDeviceService> gattServices;
        std::map<std::string, GattCharacteristic> gattCharacteristics;
        std::map<std::string, winrt::event_token> valueChangedTokens;

        BluetoothDeviceAgent(BluetoothLEDevice device, winrt::event_token connnectionStatusChangedToken)
            : device(device),
              connnectionStatusChangedToken(connnectionStatusChangedToken) {}


        ~BluetoothDeviceAgent()
        {
            device = nullptr;
        }


        IAsyncOperation<GattDeviceService> GetServiceAsync(std::string service)
        {
            if (gattServices.count(service) == 0)
            {
                auto serviceResult = co_await device.GetGattServicesAsync();
                if (serviceResult.Status() != GattCommunicationStatus::Success) co_return nullptr;

                for (auto s : serviceResult.Services())
                {
                    if (to_uuidstr(s.Uuid()) == service) gattServices.insert(std::make_pair(service, s));
                }
            }
            co_return gattServices.at(service);
        }


        IAsyncOperation<GattCharacteristic> GetCharacteristicAsync(std::string service, std::string characteristic)
        {
            if (gattCharacteristics.count(characteristic) == 0)
            {
                auto gattService = co_await GetServiceAsync(service);

                auto characteristicResult = co_await gattService.GetCharacteristicsAsync();
                if (characteristicResult.Status() != GattCommunicationStatus::Success) co_return nullptr;

                for (auto c : characteristicResult.Characteristics())
                {
                    if (to_uuidstr(c.Uuid()) == characteristic) gattCharacteristics.insert(std::make_pair(characteristic, c));
                }
            }
            co_return gattCharacteristics.at(characteristic);
        }
    };

} // namespace
