#ifndef BLUETOOTH_DEVICE_AGENT_H
#define BLUETOOTH_DEVICE_AGENT_H

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

#include "../../../lib/src/generated/bledata.pb.h"
#include "ble_utils.h"

namespace
{
    /**
     * @brief Helper/storage object for BLE device and corresponding services, characteristics, tokens, etc.
     */
    class BluetoothDeviceAgent
    {
    public:
        winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device;
        winrt::event_token connnectionStatusChangedToken;
        std::map<std::string, winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService> gattServices;  // Service UUID : GattDeviceService
        std::map<std::string, winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic> gattCharacteristics;  // Characteristic UUID : GattCharacteristic

        // (Service UUID, Characteristic UUID) : (GattCharacteristic, Event Token)
        // Key is the combination of service and characteristic UUIDs as characteristic UUID on its own is not necessarily unique globally
        std::map<std::pair<std::string, std::string>,
                 std::pair<winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic, winrt::event_token>> subscribedCharacteristicsAndTokens;

        BluetoothDeviceAgent(winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device, winrt::event_token connnectionStatusChangedToken)
            : device(device),
              connnectionStatusChangedToken(connnectionStatusChangedToken) {}


        ~BluetoothDeviceAgent()
        {
            device = nullptr;
        }

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService> GetServiceAsync(std::string service);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic> GetCharacteristicAsync(std::string service, std::string characteristic);
    };
}

#endif // BLUETOOTH_DEVICE_AGENT_H