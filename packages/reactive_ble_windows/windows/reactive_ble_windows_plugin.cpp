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

  ReactiveBleWindowsPlugin(flutter::PluginRegistrarWindows *registrar);

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

  // winrt::fire_and_forget ConnectAsync(uint64_t addr);
  // void BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args);
  // void CleanConnection(uint64_t bluetoothAddress);
  // std::map<uint64_t, std::unique_ptr<BluetoothDeviceAgent>> connectedDevices{};

  bool initialized = false;
  BluetoothLEAdvertisementWatcher bleWatcher = nullptr;
  winrt::event_token bluetoothLEWatcherReceivedToken;
  std::unique_ptr<flutter::EventSink<EncodableValue>> scan_result_sink_;
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

  auto charHandler = std::make_unique<flutter::BleCharHandler>();
  auto statusHandler = std::make_unique<flutter::BleStatusHandler>();

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

ReactiveBleWindowsPlugin::ReactiveBleWindowsPlugin(flutter::PluginRegistrarWindows *registrar) {
  auto connectedChannel =
    std::make_unique<flutter::EventChannel<EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_connected_device",
            &flutter::StandardMethodCodec::GetInstance());
  connectedHandler = std::make_unique<flutter::BleConnectedHandler>();
  connectedChannel->SetStreamHandler(std::move(connectedHandler));
}

ReactiveBleWindowsPlugin::~ReactiveBleWindowsPlugin() {
  if (bleWatcher) {
    bleWatcher.Stop();
    bleWatcher.Received(bluetoothLEWatcherReceivedToken);
    bleWatcher = nullptr;
  }
}

void ReactiveBleWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  std::string methodName = method_call.method_name();
  if (methodName.compare("initialize") == 0) {
    bleWatcher = BluetoothLEAdvertisementWatcher();
    bluetoothLEWatcherReceivedToken = bleWatcher.Received({ this, &ReactiveBleWindowsPlugin::OnAdvertisementReceived });
    result->Success();
  } else if (methodName.compare("deinitialize") == 0) {
    if (bleWatcher) {
      bleWatcher.Stop();
      bleWatcher.Received(bluetoothLEWatcherReceivedToken);
    }
    result->Success();
  } else if (methodName.compare("scanForDevices") == 0) {
    //TODO: Use scan parameters (List<Uuid> withServices, ScanMode scanMode, bool requireLocationServicesEnabled)?
    bleWatcher.Start();
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

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnListenInternal(
    const EncodableValue* arguments, std::unique_ptr<flutter::EventSink<EncodableValue>>&& events) {
  scan_result_sink_ = std::move(events);
  if (bleWatcher.Status() != BluetoothLEAdvertisementWatcherStatus::Started) {
    bleWatcher.Start();
  }
  initialized = true;
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnCancelInternal(
    const EncodableValue* arguments) {
  scan_result_sink_ = nullptr;
  if (initialized && bleWatcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started) {
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
