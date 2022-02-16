#include "include/reactive_ble_windows/bluetooth_device_agent.h"

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
     * @brief Asynchronously get the given service from the BLE device if services are not already cached.
     * 
     * @param service UUID of the desired service.
     * @return IAsyncOperation<GattDeviceService> Asynchronous object which will return the obtained GattDeviceService.
     */
    IAsyncOperation<GattDeviceService> BluetoothDeviceAgent::GetServiceAsync(std::string service)
    {
        if (gattServices.count(service) == 0)
        {
            auto serviceResult = co_await device.GetGattServicesAsync();
            if (serviceResult.Status() != GattCommunicationStatus::Success) co_return nullptr;

            for (auto s : serviceResult.Services())
            {
                if (BleUtils::GuidToString(s.Uuid()) == service) gattServices.insert(std::make_pair(service, s));
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
    IAsyncOperation<GattCharacteristic> BluetoothDeviceAgent::GetCharacteristicAsync(std::string service, std::string characteristic)
    {
        if (gattCharacteristics.count(characteristic) == 0)
        {
            auto gattService = co_await GetServiceAsync(service);

            auto characteristicResult = co_await gattService.GetCharacteristicsAsync();
            if (characteristicResult.Status() != GattCommunicationStatus::Success) co_return nullptr;

            for (auto c : characteristicResult.Characteristics())
            {
                if (BleUtils::GuidToString(c.Uuid()) == characteristic) gattCharacteristics.insert(std::make_pair(characteristic, c));
            }
        }
        co_return gattCharacteristics.at(characteristic);
    }
} // namespace
