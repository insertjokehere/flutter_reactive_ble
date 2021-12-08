#include "../lib/src/generated/bledata.pb.h"
#include "include/reactive_ble_windows/ble_status_handler.h"

namespace flutter {

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Devices::Radios;

template <typename T = EncodableValue>
class BleStatusHandlerImpl : public BleStatusHandler<T> {
 public:
  void BleStatusChangedHandler(Radio const& sender, IInspectable const& args) {
      RadioState state = sender.State();
      BleStatusInfo status = RadioStateToBleStatus(state);
      SendBleStatus(status);
  }

 protected:
  std::unique_ptr<StreamHandlerError<T>> OnListenInternal(
      const T* arguments,
      std::unique_ptr<EventSink<T>>&& events) override {
    status_result_sink_ = std::move(events);
    InitializeBleAsync();
    return nullptr;
  }

  std::unique_ptr<StreamHandlerError<T>> OnCancelInternal(
      const T* arguments) override {
    status_result_sink_ = nullptr;
    return nullptr;
  }

 private:
  winrt::fire_and_forget InitializeBleAsync() {
    //TODO: Is it worth checking to see if the bluetooth radio does not already exist?
    auto bluetoothAdapter = co_await BluetoothAdapter::GetDefaultAsync();
    bluetoothRadio = co_await bluetoothAdapter.GetRadioAsync();
    bluetoothRadio.StateChanged({this, &BleStatusHandler::BleStatusChangedHandler});
    BleStatusInfo state = RadioStateToBleStatus(bluetoothRadio.State());
    SendBleStatus(state);
  }

  void SendBleStatus(BleStatusInfo msg) {
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

  static BleStatusInfo RadioStateToBleStatus(RadioState state) {
    BleStatusInfo status;
    switch(state) {
      case RadioState::Unknown:
        status.set_status((BleStatus) unknown);
        break;
      case RadioState::On:
        status.set_status((BleStatus) ready);
        break;
      case RadioState::Off:
        status.set_status((BleStatus) poweredOff);
        break;
      case RadioState::Disabled:
        status.set_status((BleStatus) unsupported);
        break;
      default:
        // The default case should not be reached, unless the
        // RadioState enum structure gets changed.
        status.set_status((BleStatus) unknown);
        break;
    }
    return status;
  }
};

}