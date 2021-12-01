#include "include/reactive_ble_windows/ble_status_handler.h"
#include "../lib/src/generated/bledata.pb.h"

namespace flutter {

template <typename T = EncodableValue>
class BleStatusHandlerImpl : public BleStatusHandler<T> {
 protected:
  std::unique_ptr<StreamHandlerError<T>> OnListenInternal(
      const T* arguments,
      std::unique_ptr<EventSink<T>>&& events) override {
    std::cout << "+++++++++++ ON LISTEN INTERNAL +++++++++++\n" << std::endl;  // Debugging

    status_result_sink_ = std::move(events);
    // listenToBleStatus();
    return nullptr;
  }

  std::unique_ptr<StreamHandlerError<T>> OnCancelInternal(
      const T* arguments) override {
    std::cout << "========= ON CANCEL INTERNAL ==============\n" << std::endl;  // Debugging

    status_result_sink_ = nullptr;
    return nullptr;
  }

 private:
  void listenToBleStatus() {
    // status_result_sink_->Success(EncodableMap{"status", "5"});
  }
};

}