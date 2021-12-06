#include "../lib/src/generated/bledata.pb.h"
#include "include/reactive_ble_windows/ble_status_handler.h"

namespace flutter {

template <typename T = EncodableValue>
class BleStatusHandlerImpl : public BleStatusHandler<T> {
 protected:
  std::unique_ptr<StreamHandlerError<T>> OnListenInternal(
      const T* arguments,
      std::unique_ptr<EventSink<T>>&& events) override {
    std::cout << "+++++++++++ ON LISTEN INTERNAL +++++++++++\n" << std::endl;  // Debugging

    status_result_sink_ = std::move(events);
    listenToBleStatus();
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
    std::cout << "========= LISTEN TO BLE STATUS ==============\n" << std::endl;  // Debugging
    BleStatusInfo msg;
    msg.set_status(5);  // TODO: obtain system's actual bluetooth status

    size_t size = msg.ByteSizeLong();
    uint8_t *buffer = (uint8_t*) malloc(size);
    bool success = msg.SerializeToArray(buffer, size);
    if (!success) {
      std::cout << "Failed to serialize message into buffer." << std::endl;  // Debugging
      free(buffer);
      return;
    }

    EncodableList result;
    for(uint32_t i = 0; i < size; i++) {
      uint8_t value = buffer[i];
      EncodableValue encodedVal = (EncodableValue) value;
      result.push_back(encodedVal);
    }
    status_result_sink_->EventSink::Success(result);
    free(buffer);
  }
};

}