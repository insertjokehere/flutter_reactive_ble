#ifndef BLE_CONNECTED_HANDLER_H
#define BLE_CONNECTED_HANDLER_H

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

namespace flutter {

class EncodableValue;

class BleConnectedHandler : public StreamHandler<EncodableValue> {
 public:
  BleConnectedHandler() {}
  virtual ~BleConnectedHandler() = default;

  // Prevent copying.
  BleConnectedHandler(BleConnectedHandler const&) = delete;
  BleConnectedHandler& operator=(BleConnectedHandler const&) = delete;

 protected:
  
  virtual std::unique_ptr<StreamHandlerError<>> OnListenInternal(
    const EncodableValue* arguments,
    std::unique_ptr<EventSink<EncodableValue>>&& events);

  virtual std::unique_ptr<StreamHandlerError<>> OnCancelInternal(
      const EncodableValue* arguments);

};

}  // namespace flutter

#endif  // BLE_CONNECTED_HANDLER_H