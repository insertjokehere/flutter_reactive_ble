#include "include/reactive_ble_windows/ble_status_handler.h"

#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace flutter {

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Devices::Radios;
using namespace winrt::Windows::Devices::Bluetooth;

BleStatusInfo RadioStateToBleStatus(RadioState state) {
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

void BleStatusHandler::BleStatusChangedHandler(winrt::Windows::Devices::Radios::Radio const& sender, winrt::Windows::Foundation::IInspectable const& args) {
  RadioState state = sender.State();
  BleStatusInfo status = RadioStateToBleStatus(state);
  SendBleStatus(status);
}

std::unique_ptr<StreamHandlerError<EncodableValue>> BleStatusHandler::OnListenInternal(
    const EncodableValue* arguments,
    std::unique_ptr<EventSink<EncodableValue>>&& events) {
  status_result_sink_ = std::move(events);
  InitializeBleAsync();
  return nullptr;
}

std::unique_ptr<StreamHandlerError<EncodableValue>> BleStatusHandler::OnCancelInternal(
    const EncodableValue* arguments) {
  status_result_sink_ = nullptr;
  return nullptr;
}

winrt::fire_and_forget BleStatusHandler::InitializeBleAsync() {
  //TODO: Is it worth checking to see if the bluetooth radio does not already exist?
  IAsyncOperation<BluetoothAdapter> aync_op = BluetoothAdapter::GetDefaultAsync();
  BluetoothAdapter bluetoothAdapter = co_await aync_op;
  bluetoothRadio = co_await bluetoothAdapter.GetRadioAsync();
  bluetoothRadio.StateChanged({this, &BleStatusHandler::BleStatusChangedHandler});
  BleStatusInfo state = RadioStateToBleStatus(bluetoothRadio.State());
  SendBleStatus(state);
}

void BleStatusHandler::SendBleStatus(BleStatusInfo msg) {
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

}
