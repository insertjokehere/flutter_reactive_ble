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


    union uint16_t_union
    {
        uint16_t uint16;
        byte bytes[sizeof(uint16_t)];
    };


    /**
     * @brief Converts IBuffer to byte vector.
     * 
     * @param buffer The buffer to be converted.
     * @return std::vector<uint8_t> The buffer contents as bytes.
     */
    std::vector<uint8_t> to_bytevc(IBuffer buffer)
    {
        auto reader = DataReader::FromBuffer(buffer);
        auto result = std::vector<uint8_t>(reader.UnconsumedBufferLength());
        reader.ReadBytes(result);
        return result;
    }


    /**
     * @brief Parse advertised manufacturer data into bytes.
     * 
     * @param advertisement The BLE advertisement containing manufacturer data.
     * @return std::vector<uint8_t> The parsed manufacturer data as bytes.
     */
    std::vector<uint8_t> parseManufacturerData(BluetoothLEAdvertisement advertisement)
    {
        if (advertisement.ManufacturerData().Size() == 0) return std::vector<uint8_t>();

        auto manufacturerData = advertisement.ManufacturerData().GetAt(0);
        // FIXME Compat with REG_DWORD_BIG_ENDIAN
        uint8_t *prefix = uint16_t_union{manufacturerData.CompanyId()}.bytes;
        auto result = std::vector<uint8_t>{prefix, prefix + sizeof(uint16_t_union)};

        auto data = to_bytevc(manufacturerData.Data());
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }


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
        std::cout << "Scan OnListen" << std::endl;
        scan_result_sink_ = std::move(events);
        // bleWatcher = BluetoothLEAdvertisementWatcher();
        // bluetoothLEWatcherReceivedToken = bleWatcher.Received({this, &BleScanHandler::OnAdvertisementReceived});
        // bleWatcher.Start();
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

        
        // // Register event handlers before starting the watcher.
        deviceWatcherAddedToken = deviceWatcher.Added({ this, &BleScanHandler::DeviceWatcher_Added });
        deviceWatcherUpdatedToken = deviceWatcher.Updated({ this, &BleScanHandler::DeviceWatcher_Updated });
        deviceWatcherRemovedToken = deviceWatcher.Removed({ this, &BleScanHandler::DeviceWatcher_Removed });
        deviceWatcherEnumerationCompletedToken = deviceWatcher.EnumerationCompleted({ this, &BleScanHandler::DeviceWatcher_EnumerationCompleted });
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
        std::cout << "Scan OnCancel" << std::endl;
        scan_result_sink_ = nullptr;
        // if (initialized && bleWatcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started)
        // {
        //     bleWatcher.Stop();
        //     bleWatcher.Received(bluetoothLEWatcherReceivedToken);
        //     bleWatcher = nullptr;
        // }

        if (deviceWatcher != nullptr)
        {
            deviceWatcher.Stop(); // Stop watcher before unregistering event handlers.

            // Unregister the event handlers.
            deviceWatcher.Added(deviceWatcherAddedToken);
            deviceWatcher.Updated(deviceWatcherUpdatedToken);
            deviceWatcher.Removed(deviceWatcherRemovedToken);
            deviceWatcher.EnumerationCompleted(deviceWatcherEnumerationCompletedToken);
            deviceWatcher.Stopped(deviceWatcherStoppedToken);

            deviceWatcher = nullptr;
        }

        return nullptr;
    }


    /**
     * @brief Callback for when a BLE advertisement has been received.
     * 
     * @param watcher The BLE watcher which received the advertisement.
     * @param args The arguments of the received BLE advertisement.
     */
    // void BleScanHandler::OnAdvertisementReceived(
    //     BluetoothLEAdvertisementWatcher watcher,
    //     BluetoothLEAdvertisementReceivedEventArgs args)
    // {
    //     std::cout << "Scan OnAdvertisementReceived" << std::endl;
    //     if (scan_result_sink_)
    //     {
    //         auto manufacturer_data = parseManufacturerData(args.Advertisement());
    //         std::string str(manufacturer_data.begin(), manufacturer_data.end());

    //         std::stringstream sstream;
    //         sstream << std::hex << args.BluetoothAddress();
    //         std::string localName = winrt::to_string(args.Advertisement().LocalName());

    //         DeviceScanInfo info;
    //         info.set_id(std::to_string(args.BluetoothAddress()));
    //         // If the local name is empty, use a hex representation of the device address
    //         info.set_name((localName.empty()) ? sstream.str() : localName);
    //         info.set_manufacturerdata(str);
    //         info.add_serviceuuids();
    //         info.add_servicedata();
    //         info.set_rssi(args.RawSignalStrengthInDBm());
    //         SendDeviceScanInfo(info);
    //     }
    // }

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
            // auto manData = deviceInfo.Properties().TryLookup(L"System.Devices.Aep.Manufacturer");
            // winrt::hstring unboxedData = winrt::unbox_value<winrt::hstring>(manData);
            // info.set_manufacturerdata(to_string(unboxedData));
            info.add_serviceuuids();
            info.add_servicedata();
            auto property = deviceInfo.Properties().TryLookup(L"System.Devices.Aep.SignalStrength");
            if (property)
            {
                int32_t rssi = winrt::unbox_value<int32_t>(property);
                info.set_rssi(rssi);
            }
            SendDeviceScanInfo(info);
            discoveredDevices.insert_or_assign(std::to_string(btaddr), info);
        }
    }

    void BleScanHandler::DeviceWatcher_Updated(
        DeviceWatcher sender,
        DeviceInformationUpdate deviceInfoUpdate)
    {
        if (sender != deviceWatcher)
            return;
        
        auto iter = discoveredDevices.find(to_string(deviceInfoUpdate.Id()));
        if (iter == discoveredDevices.end()) return;
        DeviceScanInfo deviceInfo = iter->second;

        // auto manData = deviceInfo.Properties().TryLookup(L"System.Devices.Aep.Manufacturer");
        // winrt::hstring unboxedData = winrt::unbox_value<winrt::hstring>(manData);
        // info.set_manufacturerdata(to_string(unboxedData));

        auto property = deviceInfoUpdate.Properties().TryLookup(L"System.Devices.Aep.SignalStrength");
        if (property)
        {
            int32_t rssi = winrt::unbox_value<int32_t>(property);
            deviceInfo.set_rssi(rssi);
        }
        SendDeviceScanInfo(deviceInfo);
        discoveredDevices.insert_or_assign(deviceInfo.id(), deviceInfo);
    }

    void BleScanHandler::DeviceWatcher_Removed(
        DeviceWatcher sender,
        DeviceInformationUpdate deviceInfoUpdate)
    {
        // How should this work with Flutter?
        auto iter = discoveredDevices.find(to_string(deviceInfoUpdate.Id()));
        if (iter == discoveredDevices.end()) return;
        discoveredDevices.erase(iter);
    }

    void BleScanHandler::DeviceWatcher_EnumerationCompleted(
        DeviceWatcher sender,
        IInspectable const&)
    {
        // Is this necessary?
        std::cout << "DeviceWatcher Enumeration Completed" << std::endl;
    }

    void BleScanHandler::DeviceWatcher_Stopped(
        DeviceWatcher sender,
        IInspectable const&)
    {
        std::cout << "DeviceWatcher Stopped" << std::endl;
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
