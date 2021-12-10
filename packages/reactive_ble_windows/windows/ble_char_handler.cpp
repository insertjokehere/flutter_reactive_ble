#include "include/reactive_ble_windows/ble_char_handler.h"

#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace flutter {

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Devices::Radios;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Storage::Streams;

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleCharHandler::OnListenInternal(
    const EncodableValue* arguments, std::unique_ptr<flutter::EventSink<EncodableValue>>&& events) {
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleCharHandler::OnCancelInternal(
    const EncodableValue* arguments) {
  return nullptr;
}

}