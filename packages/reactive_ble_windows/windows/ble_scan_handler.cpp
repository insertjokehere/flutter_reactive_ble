#include "include/reactive_ble_windows/ble_scan_handler.h"

#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace flutter {

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Devices::Radios;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Storage::Streams;


union uint16_t_union {
  uint16_t uint16;
  byte bytes[sizeof(uint16_t)];
};

std::vector<uint8_t> to_bytevc(IBuffer buffer) {
  auto reader = DataReader::FromBuffer(buffer);
  auto result = std::vector<uint8_t>(reader.UnconsumedBufferLength());
  reader.ReadBytes(result);
  return result;
}

std::vector<uint8_t> parseManufacturerData(BluetoothLEAdvertisement advertisement) {
  if (advertisement.ManufacturerData().Size() == 0)
    return std::vector<uint8_t>();

  auto manufacturerData = advertisement.ManufacturerData().GetAt(0);
  // FIXME Compat with REG_DWORD_BIG_ENDIAN
  uint8_t* prefix = uint16_t_union{ manufacturerData.CompanyId() }.bytes;
  auto result = std::vector<uint8_t>{ prefix, prefix + sizeof(uint16_t_union) };

  auto data = to_bytevc(manufacturerData.Data());
  result.insert(result.end(), data.begin(), data.end());
  return result;
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleScanHandler::OnListenInternal(
    const EncodableValue* arguments, std::unique_ptr<flutter::EventSink<EncodableValue>>&& events) {
  scan_result_sink_ = std::move(events);
  bleWatcher = BluetoothLEAdvertisementWatcher();
  bluetoothLEWatcherReceivedToken = bleWatcher.Received({ this, &BleScanHandler::OnAdvertisementReceived });
  if (bleWatcher.Status() != BluetoothLEAdvertisementWatcherStatus::Started) {
    bleWatcher.Start();
  }
  initialized = true;
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleScanHandler::OnCancelInternal(
    const EncodableValue* arguments) {
  scan_result_sink_ = nullptr;
  if (initialized && bleWatcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started) {
    bleWatcher.Stop();
    bleWatcher.Received(bluetoothLEWatcherReceivedToken);
    bleWatcher = nullptr;
  }
  return nullptr;
}

void BleScanHandler::OnAdvertisementReceived(
  BluetoothLEAdvertisementWatcher watcher,
  BluetoothLEAdvertisementReceivedEventArgs args) {
  if (scan_result_sink_) {
    auto manufacturer_data = parseManufacturerData(args.Advertisement());
    std::string str(manufacturer_data.begin(), manufacturer_data.end());

    DeviceScanInfo info;
    info.set_id(std::to_string(args.BluetoothAddress()));
    info.set_name(winrt::to_string(args.Advertisement().LocalName()));
    info.set_manufacturerdata(str);
    info.add_serviceuuids();
    info.add_servicedata();
    info.set_rssi(args.RawSignalStrengthInDBm());
    SendDeviceScanInfo(info);
  }
}

void BleScanHandler::SendDeviceScanInfo(DeviceScanInfo msg) {
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
  scan_result_sink_->EventSink::Success(result);
  free(buffer);
}

}