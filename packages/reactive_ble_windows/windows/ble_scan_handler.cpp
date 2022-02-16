#include "include/reactive_ble_windows/ble_scan_handler.h"


namespace flutter
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Devices::Radios;
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
    using namespace winrt::Windows::Devices::Enumeration;
    using namespace winrt::Windows::Storage::Streams;


    /**
     * @brief Handler for OnListen to BLE scanning for devices.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @param events Unique pointer to the event sink for BLE advertisements.
     * @return std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleScanHandler::OnListenInternal(
        const EncodableValue *arguments, std::unique_ptr<flutter::EventSink<EncodableValue>> &&events)
    {
        scan_result_sink_ = std::move(events);
        initialized = true;

        auto deviceQuery = L"\
            (System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\") AND \
            (System.Devices.Aep.Bluetooth.Le.IsConnectable:=System.StructuredQueryType.Boolean#True)";

        auto requestedProperties = winrt::single_threaded_vector<winrt::hstring>({
            L"System.Devices.Aep.DeviceAddress",
            L"System.Devices.Aep.SignalStrength",
        });

        deviceWatcher = DeviceInformation::CreateWatcher(
            deviceQuery,
            requestedProperties,
            DeviceInformationKind::AssociationEndpoint);

        // Register event handlers before starting the watcher.
        deviceWatcherAddedToken = deviceWatcher.Added({ this, &BleScanHandler::DeviceWatcher_Added });
        deviceWatcherUpdatedToken = deviceWatcher.Updated({ this, &BleScanHandler::DeviceWatcher_Updated });
        deviceWatcherRemovedToken = deviceWatcher.Removed({ this, &BleScanHandler::DeviceWatcher_Removed });
        deviceWatcherStoppedToken = deviceWatcher.Stopped({ this, &BleScanHandler::DeviceWatcher_Stopped });

        deviceWatcher.Start();

        return nullptr;
    }


    /**
     * @brief Handler for cancelling scanning for BLE devices.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @return std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleScanHandler::OnCancelInternal(
        const EncodableValue *arguments)
    {
        scan_result_sink_ = nullptr;

        if (deviceWatcher != nullptr)
        {
            deviceWatcher.Stop(); // Stop watcher before unregistering event handlers.

            // Unregister the event handlers.
            deviceWatcher.Added(deviceWatcherAddedToken);
            deviceWatcher.Updated(deviceWatcherUpdatedToken);
            deviceWatcher.Removed(deviceWatcherRemovedToken);
            deviceWatcher.Stopped(deviceWatcherStoppedToken);

            deviceWatcher = nullptr;
        }

        return nullptr;
    }


    /**
     * @brief Callback for when a BLE advertisement has been received.
     * 
     * @param sender The watcher which received the advertisement.
     * @param deviceInfo The information advertised by the device.
     */
    void BleScanHandler::DeviceWatcher_Added(
        DeviceWatcher sender,
        DeviceInformation deviceInfo)
    {
        if (scan_result_sink_ && sender == deviceWatcher)
        {
            DeviceScanInfo info;
            auto addrProperty = deviceInfo.Properties().TryLookup(L"System.Devices.Aep.DeviceAddress");
            if (!addrProperty)
                return;

            auto stringId = winrt::to_string(winrt::unbox_value<winrt::hstring>(addrProperty));
            stringId.erase(std::remove(stringId.begin(), stringId.end(), ':'), stringId.end());
            uint64_t btaddr = std::strtoull(stringId.c_str(), NULL, 16);

            info.set_id(std::to_string(btaddr));
            info.set_name(to_string(deviceInfo.Name()));
            info.add_serviceuuids();
            info.add_servicedata();

            auto manData = deviceInfo.Properties().TryLookup(L"System.Devices.Aep.Manufacturer");
            if (manData)
            {
                winrt::hstring unboxedData = winrt::unbox_value<winrt::hstring>(manData);
                info.set_manufacturerdata(to_string(unboxedData));
            }

            auto rssiProperty = deviceInfo.Properties().TryLookup(L"System.Devices.Aep.SignalStrength");
            if (rssiProperty)
            {
                int32_t rssi = winrt::unbox_value<int32_t>(rssiProperty);
                info.set_rssi(rssi);
            }

            SendDeviceScanInfo(info);
            discoveredDevices.insert_or_assign(std::to_string(btaddr), info);
        }
    }


    /**
     * @brief Callback for when a device advertises updated information.
     * 
     * @param sender The watcher which received the update advertisement.
     * @param deviceInfoUpdate The updated information advertised by the device.
     */
    void BleScanHandler::DeviceWatcher_Updated(
        DeviceWatcher sender,
        DeviceInformationUpdate deviceInfoUpdate)
    {
        if (sender != deviceWatcher)
            return;

        auto iter = discoveredDevices.find(to_string(deviceInfoUpdate.Id()));
        if (iter == discoveredDevices.end()) return;
        DeviceScanInfo deviceInfo = iter->second;

        auto manData = deviceInfoUpdate.Properties().TryLookup(L"System.Devices.Aep.Manufacturer");
        if (manData)
        {
            winrt::hstring unboxedData = winrt::unbox_value<winrt::hstring>(manData);
            deviceInfo.set_manufacturerdata(to_string(unboxedData));
        }

        auto rssiProperty = deviceInfoUpdate.Properties().TryLookup(L"System.Devices.Aep.SignalStrength");
        if (rssiProperty)
        {
            int32_t rssi = winrt::unbox_value<int32_t>(rssiProperty);
            deviceInfo.set_rssi(rssi);
        }
        SendDeviceScanInfo(deviceInfo);
        discoveredDevices.insert_or_assign(deviceInfo.id(), deviceInfo);
    }


    /**
     * @brief Callback for when a previously advertised device is no longer discoverable.
     * 
     * @param sender The device watcher.
     * @param deviceInfoUpdate The device which is no longer discoverable.
     */
    void BleScanHandler::DeviceWatcher_Removed(
        DeviceWatcher sender,
        DeviceInformationUpdate deviceInfoUpdate)
    {
        // TODO: How should this work with Flutter, as the discovered devices list is managed elsewhere?
        auto iter = discoveredDevices.find(to_string(deviceInfoUpdate.Id()));
        if (iter == discoveredDevices.end()) return;
        discoveredDevices.erase(iter);
    }


    /**
     * @brief Callback for when the device watcher has been stopped. Currently no implemented behaviour.
     * 
     * @param sender The device watcher which was stopped.
     */
    void BleScanHandler::DeviceWatcher_Stopped(
        DeviceWatcher sender,
        IInspectable const&)
    {
        // No behaviour needed, but the method must be implemented.
    }


    /**
     * @brief Sends the info obtained from a BLE advertisement to the scan results channel.
     * 
     * @param msg The info of the scanned device.
     */
    void BleScanHandler::SendDeviceScanInfo(DeviceScanInfo msg)
    {
        size_t size = msg.ByteSizeLong();
        uint8_t *buffer = (uint8_t *)malloc(size);
        bool success = msg.SerializeToArray(buffer, size);
        if (!success)
        {
            std::cout << "Failed to serialize message into buffer." << std::endl; // Debugging
            free(buffer);
            return;
        }

        EncodableList result;
        for (uint32_t i = 0; i < size; i++)
        {
            uint8_t value = buffer[i];
            EncodableValue encodedVal = (EncodableValue)value;
            result.push_back(encodedVal);
        }
        scan_result_sink_->EventSink::Success(result);
        free(buffer);
    }

} // namespace flutter
