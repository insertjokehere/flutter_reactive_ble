#include "include/reactive_ble_windows/ble_char_handler.h"


namespace flutter
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Devices::Radios;
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
    using namespace winrt::Windows::Storage::Streams;


    /**
     * @brief Handler for OnListen to reading a specific characteristic from a connected device.
     * 
     * If the characteristic address and buffer of response data are both non-null, the data
     * will be sent to the characteristic stream.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @param events Unique pointer to the event sink for characteristic data.
     * @return std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleCharHandler::OnListenInternal(
        const EncodableValue *arguments, std::unique_ptr<flutter::EventSink<EncodableValue>> &&events)
    {
        characteristic_sink_ = std::move(events);
        if (*callingMethod == CallingMethod::read)
        {
            SendCharacteristicInfo();
            *callingMethod = CallingMethod::none;
        }
        else if (*callingMethod == CallingMethod::subscribe || *callingMethod == CallingMethod::unsubscribe)
        {
            bool shouldSubscribe;
            std::string errorMessage;
            if (*callingMethod == CallingMethod::subscribe)
            {
                shouldSubscribe = true;
                errorMessage = "Failed to subscribe to characteristic.";
            }
            else
            {
                shouldSubscribe = false;
                errorMessage = "Failed to unsubscribe from characteristic.";
            }
            auto task { SetNotifiableAsync(*characteristicAddress, shouldSubscribe) };
            bool success = task.get();
            if (!success)
            {
                characteristic_sink_->EventSink::Error(errorMessage);
            }
            *callingMethod = CallingMethod::none;
        }
        return nullptr;
    }


    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleCharHandler::OnCancelInternal(
        const EncodableValue *arguments)
    {
        characteristic_sink_ = nullptr;
        return nullptr;
    }


    /**
     * @brief Convert the given CharacteristicValueInfo into an EncodableList, and send the result to the characteristic event sink.
     * 
     * @param info The CharacteristicValueInfo to convert and send.
     */
    void BleCharHandler::EncodeAndSend(CharacteristicValueInfo info)
    {
        size_t size = info.ByteSizeLong();
        uint8_t *buffer = (uint8_t *)malloc(size);
        bool success = info.SerializeToArray(buffer, size);
        if (!success)
        {
            free(buffer);
            characteristic_sink_->EventSink::Error("Failed to serialize message into buffer.");  //TODO: Will this crash due to not having an error code?
            return;
        }

        EncodableList result;
        for (uint32_t i = 0; i < size; i++)
        {
            uint8_t value = buffer[i];
            EncodableValue encodedVal = (EncodableValue)value;
            result.push_back(encodedVal);
        }
        characteristic_sink_->EventSink::Success(result);
        free(buffer);
    }


    /**
     * @brief Encode and send info on a characteristic, assuming non-null address and buffer.
     */
    void BleCharHandler::SendCharacteristicInfo()
    {
        CharacteristicValueInfo info;
        info.mutable_characteristic()->CopyFrom(*characteristicAddress);

        auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(*characteristicBuffer);
        reader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
        winrt::hstring writeValue = reader.ReadString(characteristicBuffer->Length());
        std::string strVal = to_string(writeValue);

        // Protobuf expects an array of bytes, which are equivalent to strings
        for (size_t i = 0; i < writeValue.size(); i++)
        {
            info.mutable_value()->push_back(strVal[i]);
        }

        EncodeAndSend(info);
    }


    /**
     * @brief Set notification level (notify, indicate, or none) for the given characteristic (asynchronous).
     * 
     * @param charAddr The address of the characteristic to subscribe to.
     * @param shouldSubscribe If the notification should be a subscription or the cancellation of a subscription.
     * @return concurrency::task<bool> If the action was successful.
     */
    concurrency::task<bool> BleCharHandler::SetNotifiableAsync(CharacteristicAddress charAddr, bool shouldSubscribe)
    {
        return concurrency::create_task([this, charAddr, shouldSubscribe]
        {
            std::string deviceID = charAddr.deviceid();
            uint64_t addr = std::stoull(deviceID);
            auto iter = connectedDevices->find(addr);
            if (iter == connectedDevices->end()) return false;

            BluetoothDeviceAgent agent = *iter->second;
            std::string serviceUuid = BleUtils::ProtobufUuidToString(charAddr.serviceuuid());
            std::string charUuid = BleUtils::ProtobufUuidToString(charAddr.characteristicuuid());
            auto gattChar = agent.GetCharacteristicAsync(serviceUuid, charUuid).get();
            GattClientCharacteristicConfigurationDescriptorValue descriptor = GattClientCharacteristicConfigurationDescriptorValue::None;
            if (shouldSubscribe)
            {
                if ((gattChar.CharacteristicProperties() & GattCharacteristicProperties::Indicate) != GattCharacteristicProperties::None)
                {
                    descriptor = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
                }
                else if ((gattChar.CharacteristicProperties() & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None)
                {
                    descriptor = GattClientCharacteristicConfigurationDescriptorValue::Notify;
                }
            }
            auto writeDescriptorStatus = gattChar.WriteClientCharacteristicConfigurationDescriptorAsync(descriptor).get();
            if (writeDescriptorStatus != GattCommunicationStatus::Success) return false;

            std::pair<std::string, std::string> keyPair = std::make_pair(serviceUuid, charUuid);
            if (shouldSubscribe)
            {
                winrt::event_token token = gattChar.ValueChanged({this, &BleCharHandler::GattCharacteristic_ValueChanged});
                std::pair<GattCharacteristic, winrt::event_token> valuePair = std::make_pair(gattChar, token);
                agent.subscribedCharacteristicsAndTokens.insert(std::make_pair(keyPair, valuePair));
            }
            else
            {
                std::pair<GattCharacteristic, winrt::event_token> valuePair = agent.subscribedCharacteristicsAndTokens.at(keyPair);
                // Unregister handler
                valuePair.first.ValueChanged(valuePair.second);
                agent.subscribedCharacteristicsAndTokens.erase(keyPair);
            }

            // Update connectedDevices map with changed agent
            connectedDevices->insert_or_assign(addr, std::make_unique<BluetoothDeviceAgent>(agent));
            return true;
        });
    }


    /**
     * @brief Callback method for when a subscribed characteristic has changed.
     * 
     * Relies on the characteristic_sink_ being set in the OnListen method.
     * If it is null, the method returns early as it cannot proceed.
     * 
     * @param sender The notifying characteristic.
     * @param args The arguments of the changed characteristic.
     */
    winrt::fire_and_forget BleCharHandler::GattCharacteristic_ValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        if (characteristic_sink_ == nullptr)
        {
            //TODO: Is there a way the sink can be handled to avoid this case? Currently very fragile
            std::cerr << "Error: Characteristic sink not yet initialized." << std::endl;
            co_return;
        }

        IBuffer characteristicValue = args.CharacteristicValue();
        auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(characteristicValue);
        reader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);

        GattDeviceService service = sender.Service();
        uint64_t bluetoothAddr = service.Device().BluetoothAddress();
        std::string addrString = to_string(winrt::to_hstring(bluetoothAddr));

        CharacteristicAddress charAddr;
        charAddr.mutable_deviceid()->assign(addrString);

        std::vector<uint8_t> charUuidBytes = BleUtils::GuidToByteVec(sender.Uuid());
        for (auto i = charUuidBytes.begin(); i != charUuidBytes.end(); i++)
            charAddr.mutable_characteristicuuid()->mutable_data()->push_back(*i);

        std::vector<uint8_t> serviceUuidBytes = BleUtils::GuidToByteVec(service.Uuid());
        for (auto j = serviceUuidBytes.begin(); j != serviceUuidBytes.end(); j++)
            charAddr.mutable_serviceuuid()->mutable_data()->push_back(*j);

        CharacteristicValueInfo updatedCharacteristic;
        updatedCharacteristic.mutable_characteristic()->CopyFrom(charAddr);

        int length = characteristicValue.Length();
        std::vector<uint8_t> data(length);
        reader.ReadBytes(data);
        updatedCharacteristic.set_value(std::string (data.begin(), data.end()));

        EncodeAndSend(updatedCharacteristic);
    }

} // namespace flutter
