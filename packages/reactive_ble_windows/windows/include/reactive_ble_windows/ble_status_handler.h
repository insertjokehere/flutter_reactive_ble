#ifndef BLE_STATUS_HANDLER_H
#define BLE_STATUS_HANDLER_H

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler.h>
#include <flutter/standard_method_codec.h>

namespace flutter {

class EncodableValue;

template <typename T = EncodableValue>
class BleStatusHandler : public StreamHandler<T> {
 public:
  BleStatusHandler() {}
  virtual ~BleStatusHandler() = default;

  // Prevent copying.
  BleStatusHandler(BleStatusHandler const&) = delete;
  BleStatusHandler& operator=(BleStatusHandler const&) = delete;

  std::unique_ptr<flutter::EventSink<EncodableValue>> status_result_sink_;

 protected:
  virtual std::unique_ptr<StreamHandlerError<>> OnListenInternal(
    const T* arguments,
    std::unique_ptr<EventSink<T>>&& events) = 0;

  virtual std::unique_ptr<StreamHandlerError<>> OnCancelInternal(
      const T* arguments) = 0;

 private:
  virtual void listenToBleStatus() = 0;
};

}  // namespace flutter

#endif  // BLE_STATUS_HANDLER_H