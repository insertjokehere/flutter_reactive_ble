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

#include "../lib/src/generated/bledata.pb.h"
#include "include/reactive_ble_windows/ble_utils.h"

namespace
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Storage::Streams;
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    using flutter::EncodableMap;
    using flutter::EncodableValue;


    /**
     * @brief Helper/storage object for BLE device and corresponding services, characteristics, tokens.
     */
    struct BluetoothDeviceAgent
    {
        BluetoothLEDevice device;
        winrt::event_token connnectionStatusChangedToken;
        std::map<std::string, GattDeviceService> gattServices;
        std::map<std::string, GattCharacteristic> gattCharacteristics;
        std::map<std::string, winrt::event_token> valueChangedTokens;
        std::map<uint64_t, CharacteristicAddress> subscribedCharacteristicsAddresses;

        BluetoothDeviceAgent(BluetoothLEDevice device, winrt::event_token connnectionStatusChangedToken)
            : device(device),
              connnectionStatusChangedToken(connnectionStatusChangedToken) {}


        ~BluetoothDeviceAgent()
        {
            device = nullptr;
        }


        /**
         * @brief Asynchronously get the given service from the BLE device if services are not already cached.
         * 
         * @param service UUID of the desired service.
         * @return IAsyncOperation<GattDeviceService> Asynchronous object which will return the obtained GattDeviceService.
         */
        IAsyncOperation<GattDeviceService> GetServiceAsync(std::string service)
        {
            if (gattServices.count(service) == 0)
            {
                auto serviceResult = co_await device.GetGattServicesAsync();
                if (serviceResult.Status() != GattCommunicationStatus::Success) co_return nullptr;

                for (auto s : serviceResult.Services())
                {
                    if (BleUtils::to_uuidstr(s.Uuid()) == service) gattServices.insert(std::make_pair(service, s));
                }
            }
            co_return gattServices.at(service);
        }


        /**
         * @brief Asynchronously get the given characteristic from the BLE device if characteristics are not already cached.
         * 
         * @param service UUID of the service which has the desired characteristic.
         * @param characteristic UUID of the desired characteristic.
         * @return IAsyncOperation<GattCharacteristic> Asynchronous object which will return the obtained GattCharacteristic.
         */
        IAsyncOperation<GattCharacteristic> GetCharacteristicAsync(std::string service, std::string characteristic)
        {
            if (gattCharacteristics.count(characteristic) == 0)
            {
                auto gattService = co_await GetServiceAsync(service);

                auto characteristicResult = co_await gattService.GetCharacteristicsAsync();
                if (characteristicResult.Status() != GattCommunicationStatus::Success) co_return nullptr;

                for (auto c : characteristicResult.Characteristics())
                {
                    if (BleUtils::to_uuidstr(c.Uuid()) == characteristic) gattCharacteristics.insert(std::make_pair(characteristic, c));
                }
            }
            co_return gattCharacteristics.at(characteristic);
        }
    };

} // namespace
