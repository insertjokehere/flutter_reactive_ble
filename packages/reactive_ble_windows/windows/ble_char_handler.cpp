#include "include/reactive_ble_windows/ble_char_handler.h"

#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>

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
        std::cout << "on listen" << std::endl;
        characteristic_sink_ = std::move(events);
        // if (characteristicAddress != nullptr && characteristicBuffer != nullptr)
        // {
        //     SendCharacteristicInfo();
        // }
        return nullptr;
    }


    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleCharHandler::OnCancelInternal(
        const EncodableValue *arguments)
    {
        std::cout << "on cancel" << std::endl;
        characteristic_sink_ = nullptr;
        return nullptr;
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

} // namespace flutter
