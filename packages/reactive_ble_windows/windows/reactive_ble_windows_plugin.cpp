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

#include "ble_status_handler.cpp"

namespace {

using flutter::EncodableMap;
using flutter::EncodableValue;

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

  auto statusHandler = std::make_unique<flutter::BleStatusHandlerImpl<>>();

  connectedChannel->SetStreamHandler(std::move(handler));
  characteristicChannel->SetStreamHandler(std::move(handler));
  scanChannel->SetStreamHandler(std::move(handler));
  statusChannel->SetStreamHandler(std::move(statusHandler));

  registrar->AddPlugin(std::move(plugin));
}

std::vector<uint8_t> parseManufacturerData(BluetoothLEAdvertisement advertisement)
{
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
  } else {
    result->NotImplemented();
  }
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnListenInternal(
    const EncodableValue* arguments, std::unique_ptr<flutter::EventSink<EncodableValue>>&& events)
{
  auto args = std::get<EncodableMap>(*arguments);
  auto name = std::get<std::string>(args[EncodableValue("name")]);
  if (name.compare("scanResult") == 0) {
    scan_result_sink_ = std::move(events);
  }
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnCancelInternal(
    const EncodableValue* arguments)
{
  auto args = std::get<EncodableMap>(*arguments);
  auto name = std::get<std::string>(args[EncodableValue("name")]);
  if (name.compare("scanResult") == 0) {
      scan_result_sink_ = nullptr;
  }
  return nullptr;
}

void ReactiveBleWindowsPlugin::OnAdvertisementReceived(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  // std::cout << winrt::to_string(args.Advertisement().LocalName()) << std::endl;  // Debugging
  auto manufacturer_data = parseManufacturerData(args.Advertisement());
  if (scan_result_sink_) {
    scan_result_sink_->Success(EncodableMap{
      {"name", winrt::to_string(args.Advertisement().LocalName())},
      {"deviceId", std::to_string(args.BluetoothAddress())},
      {"manufacturerData", manufacturer_data},
      {"rssi", args.RawSignalStrengthInDBm()},
    });
  }
}

}  // namespace

void ReactiveBleWindowsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  ReactiveBleWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
