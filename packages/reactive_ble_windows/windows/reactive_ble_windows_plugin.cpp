#include "include/reactive_ble_windows/reactive_ble_windows_plugin.h"
#pragma comment( lib, "windowsapp")

// This must be included before many other Windows headers.
#include <windows.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>

#include "../lib/src/generated/bledata.pb.h"
#include "include/reactive_ble_windows/ble_connected_handler.h"
#include "include/reactive_ble_windows/ble_char_handler.h"
#include "include/reactive_ble_windows/ble_status_handler.h"

namespace {

using flutter::EncodableMap;
using flutter::EncodableValue;
using flutter::EncodableList;

using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Foundation;
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

class ReactiveBleWindowsPlugin : public flutter::Plugin, public flutter::StreamHandler<EncodableValue> {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  ReactiveBleWindowsPlugin();

  virtual ~ReactiveBleWindowsPlugin();

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs args);

  std::unique_ptr<flutter::StreamHandlerError<>> OnListenInternal(
      const EncodableValue* arguments,
      std::unique_ptr<flutter::EventSink<>>&& events) override;

  std::unique_ptr<flutter::StreamHandlerError<>> OnCancelInternal(
      const EncodableValue* arguments) override;

  void SendDeviceScanInfo(DeviceScanInfo msg);

  BluetoothLEAdvertisementWatcher bleWatcher{ nullptr };
  winrt::event_token bluetoothLEWatcherReceivedToken;
  std::unique_ptr<flutter::EventSink<EncodableValue>> scan_result_sink_;
};

void ReactiveBleWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto methodChannel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "flutter_reactive_ble_method",
          &flutter::StandardMethodCodec::GetInstance());
  
  auto connectedChannel =
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_connected_device",
            &flutter::StandardMethodCodec::GetInstance());

  auto characteristicChannel =
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_char_update",
            &flutter::StandardMethodCodec::GetInstance());

  auto scanChannel = 
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_scan",
            &flutter::StandardMethodCodec::GetInstance());

  auto statusChannel =
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_status",
            &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<ReactiveBleWindowsPlugin>();

  methodChannel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  auto handler = std::make_unique<
      flutter::StreamHandlerFunctions<>>(
      [plugin_pointer = plugin.get()](
          const EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<>>&& events)
          -> std::unique_ptr<flutter::StreamHandlerError<>> {
        return plugin_pointer->OnListen(arguments, std::move(events));
      },
      [plugin_pointer = plugin.get()](const EncodableValue* arguments)
          -> std::unique_ptr<flutter::StreamHandlerError<>> {
        return plugin_pointer->OnCancel(arguments);
      });

  auto connectedHandler = std::make_unique<flutter::BleConnectedHandler>();
  auto charHandler = std::make_unique<flutter::BleCharHandler>();
  auto statusHandler = std::make_unique<flutter::BleStatusHandler>();

  connectedChannel->SetStreamHandler(std::move(connectedHandler));
  characteristicChannel->SetStreamHandler(std::move(charHandler));
  scanChannel->SetStreamHandler(std::move(handler));
  statusChannel->SetStreamHandler(std::move(statusHandler));

  registrar->AddPlugin(std::move(plugin));
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

ReactiveBleWindowsPlugin::ReactiveBleWindowsPlugin() {}

ReactiveBleWindowsPlugin::~ReactiveBleWindowsPlugin() {}

void ReactiveBleWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare("initialize") == 0) {
    if (!bleWatcher) {
      bleWatcher = BluetoothLEAdvertisementWatcher();
      bluetoothLEWatcherReceivedToken = bleWatcher.Received({ this, &ReactiveBleWindowsPlugin::OnAdvertisementReceived });
    }
    result->Success();
  } else if (method_call.method_name().compare("deinitialize") == 0) {
    if (bleWatcher) {
      bleWatcher.Stop();
      bleWatcher.Received(bluetoothLEWatcherReceivedToken);
    }
    result->Success();
  } else if (method_call.method_name().compare("scanForDevices") == 0) {
    bleWatcher.Start();
    result->Success();
  } else {
    std::cout << "Unknown method: " << method_call.method_name() << std::endl;  // Debugging
    result->NotImplemented();
  }
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnListenInternal(
    const EncodableValue* arguments, std::unique_ptr<flutter::EventSink<EncodableValue>>&& events) {
  scan_result_sink_ = std::move(events);
  if (bleWatcher) {
    bleWatcher.Start();
  }
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnCancelInternal(
    const EncodableValue* arguments) {
  scan_result_sink_ = nullptr;
  if (bleWatcher) {
    bleWatcher.Stop();
  }
  return nullptr;
}

void ReactiveBleWindowsPlugin::OnAdvertisementReceived(
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

void ReactiveBleWindowsPlugin::SendDeviceScanInfo(DeviceScanInfo msg) {
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

}  // namespace

void ReactiveBleWindowsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  ReactiveBleWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
