#include "include/reactive_ble_windows/ble_char_handler.h"

#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "include/reactive_ble_windows/ble_utils.h"


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
        else if (*callingMethod == CallingMethod::notify)
        {
            auto task { SetNotifiableAsync(*characteristicAddress, GattClientCharacteristicConfigurationDescriptorValue::Notify) };
            bool success = task.get();
            if (!success)
            {
                characteristic_sink_->EventSink::Error("Failed to subscribe to characteristic.");
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
            std::cout << "Failed to serialize message into buffer." << std::endl; // Debugging
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
     * @return concurrency::task<bool> If the action was successful.
     */
    concurrency::task<bool> BleCharHandler::SetNotifiableAsync(CharacteristicAddress charAddr, GattClientCharacteristicConfigurationDescriptorValue descriptor)
    {
        std::cout << "Set notifiable" << std::endl;
        return concurrency::create_task([this, charAddr, descriptor]
        {
            std::string deviceID = charAddr.deviceid();
            uint64_t addr = std::stoull(deviceID);
            auto iter = connectedDevices->find(addr);
            if (iter == connectedDevices->end()) return false;

            BluetoothDeviceAgent agent = *iter->second;
            auto gattChar = agent.GetCharacteristicAsync(charAddr.serviceuuid().data(), charAddr.characteristicuuid().data()).get();
            auto writeDescriptorStatus = gattChar.WriteClientCharacteristicConfigurationDescriptorAsync(descriptor).get();
            if (writeDescriptorStatus != GattCommunicationStatus::Success) return false;

            if (descriptor == GattClientCharacteristicConfigurationDescriptorValue::Notify)
            {
                agent.subscribedCharacteristicsAddresses[addr] = charAddr;
                agent.valueChangedTokens[deviceID] = gattChar.ValueChanged({this, &BleCharHandler::GattCharacteristic_ValueChanged});
            }
            else
            {
                // Remove token to stop receiving notifications
                gattChar.ValueChanged(std::exchange(agent.valueChangedTokens[deviceID], {}));
                agent.subscribedCharacteristicsAddresses.erase(addr);
            }

            // Update connectedDevices map with changed agent
            connectedDevices->insert_or_assign(addr, std::make_unique<BluetoothDeviceAgent>(agent));
            return true;
        });
    }


    void BleCharHandler::GattCharacteristic_ValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        if (characteristic_sink_ == nullptr)
        {
            //TODO: Is there a way the sink can be handled to avoid this case? Currently very fragile
            std::cerr << "Error: Characteristic sink not yet initialized." << std::endl;
            return;
        }

        std::string uuid = BleUtils::to_uuidstr(sender.Uuid());
        IBuffer characteristicValue = args.CharacteristicValue();
        auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(characteristicValue);
        reader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
        winrt::hstring writeValue = reader.ReadString(characteristicValue.Length());
        std::string strVal = to_string(writeValue);

        GattDeviceService service = sender.Service();
        uint64_t bluetoothAddr = service.Device().BluetoothAddress();
        std::string addrString = to_string(winrt::to_hstring(bluetoothAddr));
        std::string serviceUUID = BleUtils::to_uuidstr(service.Uuid());

        // Retrieve old characteristic address
        auto iter = connectedDevices->find(bluetoothAddr);
        if (iter == connectedDevices->end())
        {
            characteristic_sink_->EventSink::Error("Not currently connected to device.");
            return;
        }
        auto subscribedMap = (*iter->second).subscribedCharacteristicsAddresses;
        auto subIter = subscribedMap.find(bluetoothAddr);
        if (subIter == subscribedMap.end())
        {
            characteristic_sink_->EventSink::Error("Could not obtain address of characteristic for received notification.");
            return;
        }
        CharacteristicAddress charAddr = subIter->second;

        // Update characteristic address with changed values
        charAddr.mutable_characteristicuuid()->set_data(uuid);
        charAddr.mutable_serviceuuid()->set_data(serviceUUID);

        CharacteristicValueInfo updatedCharacteristic;
        updatedCharacteristic.mutable_characteristic()->CopyFrom(charAddr);
        for (size_t i = 0; i < writeValue.size(); i++)
        {
            updatedCharacteristic.mutable_value()->push_back(strVal[i]);
        }

        EncodeAndSend(updatedCharacteristic);
    }

} // namespace flutter
