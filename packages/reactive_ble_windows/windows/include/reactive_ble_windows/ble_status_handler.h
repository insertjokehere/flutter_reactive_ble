#ifndef BLE_STATUS_HANDLER_H
#define BLE_STATUS_HANDLER_H

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler.h>
#include <flutter/standard_method_codec.h>

#include <windows.h>
#include <winrt/windows.devices.bluetooth.h>
#include <winrt/windows.devices.radios.h>

#include "../lib/src/generated/bledata.pb.h"

namespace flutter
{

class EncodableValue;

enum BleStatus
{
  unknown = 0,
  unsupported = 1,
  unauthorized = 2,
  poweredOff = 3,
  locationServicesDisabled = 4,
  ready = 5
};

class BleStatusHandler : public StreamHandler<EncodableValue>
{
 public:
  BleStatusHandler() {}
  virtual ~BleStatusHandler() = default;

  // Prevent copying.
  BleStatusHandler(BleStatusHandler const&) = delete;
  BleStatusHandler& operator=(BleStatusHandler const&) = delete;

  std::unique_ptr<flutter::EventSink<EncodableValue>> status_result_sink_;

  virtual void BleStatusChangedHandler(winrt::Windows::Devices::Radios::Radio const& sender,
    winrt::Windows::Foundation::IInspectable const& args);

 protected:
  winrt::Windows::Devices::Radios::Radio bluetoothRadio{ nullptr };
  
  virtual std::unique_ptr<StreamHandlerError<>> OnListenInternal(
    const EncodableValue* arguments,
    std::unique_ptr<EventSink<EncodableValue>>&& events);

  virtual std::unique_ptr<StreamHandlerError<>> OnCancelInternal(
      const EncodableValue* arguments);

 private:
  virtual winrt::fire_and_forget InitializeBleAsync();

  virtual void SendBleStatus(BleStatusInfo msg);
};

}  // namespace flutter

#endif  // BLE_STATUS_HANDLER_H
