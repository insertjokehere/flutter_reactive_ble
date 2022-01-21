#include "include/reactive_ble_windows/ble_scan_handler.h"


namespace flutter
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Devices::Radios;
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
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
        scan_result_sink_ = std::move(events);
        bleWatcher = BluetoothLEAdvertisementWatcher();
        bluetoothLEWatcherReceivedToken = bleWatcher.Received({this, &BleScanHandler::OnAdvertisementReceived});
        bleWatcher.Start();
        initialized = true;
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
        if (initialized && bleWatcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started)
        {
            bleWatcher.Stop();
            bleWatcher.Received(bluetoothLEWatcherReceivedToken);
            bleWatcher = nullptr;
        }
        return nullptr;
    }


    concurrency::task<std::string> GetNameAsync(uint64_t addr)
    {
        return concurrency::create_task([addr]
        {
            std::string name = "";
            BluetoothLEDevice device = BluetoothLEDevice::FromBluetoothAddressAsync(addr).get();
            if (device == nullptr)
                return name;
            auto serviceResult = device.GetGattServicesAsync().get();

            bool readNameChar = false;
            for (const GattDeviceService _tmp : serviceResult.Services())
            {
                // 0x1800 is the GAP service
                if (_tmp.Uuid().Data1 == 6144)
                {
                    auto charResult = _tmp.GetCharacteristicsAsync().get();
                    for (const GattCharacteristic _tmp2 : charResult.Characteristics())
                    {
                        // 0x2A00 is the device name characteristic
                        if (_tmp2.Uuid().Data1 == 10752)
                        {
                            auto value = _tmp2.ReadValueAsync().get();
                            IBuffer val = value.Value();

                            auto reader = DataReader::FromBuffer(val);
                            auto result = std::vector<uint8_t>(reader.UnconsumedBufferLength());
                            reader.ReadBytes(result);
                            name = std::string(result.begin(), result.end());
                            // There is the potential for the name to still be empty, so break on
                            // the knowledge that the name characteristic has been read
                            readNameChar = true;
                            break;
                        } 
                    }
                }

                if (readNameChar)
                    break;
            }
            device.Close();
            return name;
        });
    }


    /**
     * @brief Callback for when a BLE advertisement has been received.
     * 
     * @param watcher The BLE watcher which received the advertisement.
     * @param args The arguments of the received BLE advertisement.
     */
    void BleScanHandler::OnAdvertisementReceived(
        BluetoothLEAdvertisementWatcher watcher,
        BluetoothLEAdvertisementReceivedEventArgs args)
    {
        if (scan_result_sink_)
        {
            auto manufacturer_data = parseManufacturerData(args.Advertisement());
            std::string str(manufacturer_data.begin(), manufacturer_data.end());

            std::stringstream sstream;
            sstream << std::hex << args.BluetoothAddress();
            std::string localName = winrt::to_string(args.Advertisement().LocalName());

            if (localName.empty())
                localName = GetNameAsync(args.BluetoothAddress()).get();

            DeviceScanInfo info;
            info.set_id(std::to_string(args.BluetoothAddress()));
            // If the local name is still empty, use a hex representation of the device address
            info.set_name((localName.empty()) ? sstream.str() : localName);
            info.set_manufacturerdata(str);
            info.add_serviceuuids();
            info.add_servicedata();
            info.set_rssi(args.RawSignalStrengthInDBm());
            SendDeviceScanInfo(info);
        }
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
