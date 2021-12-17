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
#include <variant>

#include "../lib/src/generated/bledata.pb.h"
#include "include/reactive_ble_windows/ble_connected_handler.h"
#include "include/reactive_ble_windows/ble_char_handler.h"
#include "include/reactive_ble_windows/ble_scan_handler.h"
#include "include/reactive_ble_windows/ble_status_handler.h"
// #include "bluetooth_device_agent.cpp"

namespace {

using flutter::EncodableMap;
using flutter::EncodableValue;
using flutter::EncodableList;

using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;

class ReactiveBleWindowsPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  ReactiveBleWindowsPlugin(flutter::PluginRegistrarWindows *registrar);

  virtual ~ReactiveBleWindowsPlugin();

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  
  // winrt::fire_and_forget ConnectAsync(uint64_t addr);
  // void BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args);
  // void CleanConnection(uint64_t bluetoothAddress);
  // std::map<uint64_t, std::unique_ptr<BluetoothDeviceAgent>> connectedDevices{};

  std::unique_ptr<flutter::StreamHandler<flutter::EncodableValue>> scanHandler;
  std::unique_ptr<flutter::StreamHandler<flutter::EncodableValue>> connectedHandler;
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

  auto plugin = std::make_unique<ReactiveBleWindowsPlugin>(registrar);

  methodChannel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  auto charHandler = std::make_unique<flutter::BleCharHandler>();
  auto scanHandler = std::make_unique<flutter::BleScanHandler>();
  auto statusHandler = std::make_unique<flutter::BleStatusHandler>();

  characteristicChannel->SetStreamHandler(std::move(charHandler));
  scanChannel->SetStreamHandler(std::move(scanHandler));
  statusChannel->SetStreamHandler(std::move(statusHandler));

  registrar->AddPlugin(std::move(plugin));
}

ReactiveBleWindowsPlugin::ReactiveBleWindowsPlugin(flutter::PluginRegistrarWindows *registrar) {
  auto connectedChannel =
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_connected_device",
            &flutter::StandardMethodCodec::GetInstance());
  connectedHandler = std::make_unique<flutter::BleConnectedHandler>();
  connectedChannel->SetStreamHandler(std::move(connectedHandler));

  auto scanChannel =
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_scan",
            &flutter::StandardMethodCodec::GetInstance());
  scanHandler = std::make_unique<flutter::BleScanHandler>();
  scanChannel->SetStreamHandler(std::move(scanHandler));
}

ReactiveBleWindowsPlugin::~ReactiveBleWindowsPlugin() {}

void ReactiveBleWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  std::string methodName = method_call.method_name();
  if (methodName.compare("initialize") == 0) {  //TODO: Is anything needed in initialize, deinitialize, and scanForDevices now?
    result->Success();
  } else if (methodName.compare("deinitialize") == 0) {
    result->Success();
  } else if (methodName.compare("scanForDevices") == 0) {
    //TODO: Use scan parameters (List<Uuid> withServices, ScanMode scanMode, bool requireLocationServicesEnabled)?
    result->Success();
  } else if (methodName.compare("connectToDevice") == 0) {
    ConnectToDeviceRequest req;

    // std::get_if returns a pointer to the value stored or a null pointer on error. 
    // This ensures we return early if we get a null pointer.
    const flutter::EncodableValue* pEncodableValue = method_call.arguments();
    const std::vector<uint8_t>* pVector = std::get_if<std::vector<uint8_t>>(pEncodableValue);
    if (pVector && pVector->size() > 0) {
      // Parse vector into a protobuf message. Note the call to `pVector->data()` (https://en.cppreference.com/w/cpp/container/vector/data),
      // and note further that this may return a null pointer if pVector->size() is 0.
      bool res = req.ParsePartialFromArray(pVector->data(), pVector->size());
      if (res) {

        std::cout << "Connect to device request: " << req.DebugString() << std::flush;  // Apparently includes a newline char.
        uint64_t addr = std::stoull(req.deviceid());
        // ConnectAsync(addr);

      } else {
        result->Error("Unable to parse message");
        return;
      }
    } else {
      result->Error("No data");
      return;
    }

    result->Success();
  } else if (methodName.compare("disconnectFromDevice") == 0) {
    // deviceId
    //TODO: Implement disconnect from device
    result->NotImplemented();
  } else {
    std::cout << "Unknown method: " << methodName << std::endl;  // Debugging
    result->NotImplemented();
  }
}

// winrt::fire_and_forget ReactiveBleWindowsPlugin::ConnectAsync(uint64_t addr) {
//   auto device = co_await BluetoothLEDevice::FromBluetoothAddressAsync(addr);
//   auto servicesResult = co_await device.GetGattServicesAsync();
//   if (servicesResult.Status() != GattCommunicationStatus::Success) {
//     OutputDebugString((L"GetGattServicesAsync error: " + winrt::to_hstring((int32_t)servicesResult.Status()) + L"\n").c_str());
//     // connected_device_sink_->Send(EncodableMap{
//     //       {"deviceId", std::to_string(addr)},
//     //       {"ConnectionState", "disconnected"},
//     // });
//     co_return;
//   }
//   auto connnectionStatusChangedToken = device.ConnectionStatusChanged({this, &ReactiveBleWindowsPlugin::BluetoothLEDevice_ConnectionStatusChanged});
//   auto deviceAgent = std::make_unique<BluetoothDeviceAgent>(device, connnectionStatusChangedToken);
//   auto pair = std::make_pair(addr, std::move(deviceAgent));
//   connectedDevices.insert(std::move(pair));
//   // connected_device_sink_->Send(EncodableMap{
//   //       {"deviceId", std::to_string(addr)},
//   //       {"ConnectionState", "connected"},
//   // });
// }


// void ReactiveBleWindowsPlugin::BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args) {
//   OutputDebugString((L"ConnectionStatusChanged " + winrt::to_hstring((int32_t)sender.ConnectionStatus()) + L"\n").c_str());
//   if (sender.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) {
//     CleanConnection(sender.BluetoothAddress());
//     // connected_device_sink_->Send(EncodableMap{
//     //       {"deviceId", std::to_string(addr)},
//     //       {"ConnectionState", "disconnected"},
//     // });
//   }
// }

// void ReactiveBleWindowsPlugin::CleanConnection(uint64_t bluetoothAddress) {
//   auto node = connectedDevices.extract(bluetoothAddress);
//   if (!node.empty()) {
//     auto deviceAgent = std::move(node.mapped());
//     deviceAgent->device.ConnectionStatusChanged(deviceAgent->connnectionStatusChangedToken);
//     for (auto &tokenPair : deviceAgent->valueChangedTokens)
//     {
//       deviceAgent->gattCharacteristics.at(tokenPair.first).ValueChanged(tokenPair.second);
//     }
//   }
// }

}  // namespace

void ReactiveBleWindowsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  ReactiveBleWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
