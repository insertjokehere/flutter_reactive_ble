#ifndef BLE_CHAR_HANDLER_H
#define BLE_CHAR_HANDLER_H

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler.h>
#include <flutter/standard_method_codec.h>

#include <windows.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>

#include "../lib/src/generated/bledata.pb.h"

namespace flutter
{
    class EncodableValue;


    class BleCharHandler : public StreamHandler<EncodableValue>
    {
    public:
        BleCharHandler() {}
        virtual ~BleCharHandler() = default;

        BleCharHandler(CharacteristicAddress* charAddr, winrt::Windows::Storage::Streams::IBuffer* buf)
        {
            characteristicAddress = charAddr;
            characteristicBuffer = buf;
        };

        // Prevent copying.
        BleCharHandler(BleCharHandler const &) = delete;
        BleCharHandler &operator=(BleCharHandler const &) = delete;

    protected:
        virtual std::unique_ptr<StreamHandlerError<>> OnListenInternal(
            const EncodableValue *arguments,
            std::unique_ptr<EventSink<EncodableValue>> &&events);

        virtual std::unique_ptr<StreamHandlerError<>> OnCancelInternal(
            const EncodableValue *arguments);

        void SendCharacteristicInfo();

        std::unique_ptr<flutter::EventSink<EncodableValue>> characteristic_sink_;
        CharacteristicAddress* characteristicAddress;
        winrt::Windows::Storage::Streams::IBuffer* characteristicBuffer;
    };

} // namespace flutter

#endif // BLE_CHAR_HANDLER_H
