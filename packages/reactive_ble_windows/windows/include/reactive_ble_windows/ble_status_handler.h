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

 protected:
  std::unique_ptr<StreamHandlerError<T>> OnListenInternal(
      const T* arguments,
      std::unique_ptr<EventSink<T>>&& events) override {
    std::cout << "+++++++++++ ON LISTEN INTERNAL +++++++++++\n" << std::endl;
    return nullptr;
  }

  std::unique_ptr<StreamHandlerError<T>> OnCancelInternal(
      const T* arguments) override {
    std::cout << "========= ON CANCEL INTERNAL ==============\n" << std::endl;
    return nullptr;
  }
};

}  // namespace flutter

#endif  // BLE_STATUS_HANDLER_H