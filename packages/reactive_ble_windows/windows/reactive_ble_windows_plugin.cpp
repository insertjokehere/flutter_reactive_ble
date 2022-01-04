#include "include/reactive_ble_windows/reactive_ble_windows_plugin.h"
#pragma comment(lib, "windowsapp")

// This must be included before many other Windows headers.
#include <windows.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <ppltasks.h>

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
#include "include/reactive_ble_windows/ble_char_handler.h"
#include "include/reactive_ble_windows/ble_scan_handler.h"
#include "include/reactive_ble_windows/ble_status_handler.h"
#include "bluetooth_device_agent.cpp"

namespace
{
    using flutter::EncodableList;
    using flutter::EncodableMap;
    using flutter::EncodableValue;

    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Storage::Streams;


    // Obtained from reactive_ble_platform_interface, connection_state_update.dart
    enum DeviceConnectionState
    {
        connecting,
        connected,
        disconnecting,
        disconnected
    };


    class ReactiveBleWindowsPlugin : public flutter::Plugin, public flutter::StreamHandler<EncodableValue>
    {
    public:
        static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

        ReactiveBleWindowsPlugin(flutter::PluginRegistrarWindows *registrar);

        virtual ~ReactiveBleWindowsPlugin();

    private:
        void HandleMethodCall(
            const flutter::MethodCall<flutter::EncodableValue> &method_call,
            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

        std::unique_ptr<flutter::StreamHandlerError<>> OnListenInternal(
            const EncodableValue *arguments,
            std::unique_ptr<flutter::EventSink<>> &&events) override;

        std::unique_ptr<flutter::StreamHandlerError<>> OnCancelInternal(
            const EncodableValue *arguments) override;

        winrt::fire_and_forget ConnectAsync(uint64_t addr);

        concurrency::task<std::list<DiscoveredService>> DiscoverServicesAsync(BluetoothDeviceAgent &bluetoothDeviceAgent, uint64_t addr);

        void BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args);

        void CleanConnection(uint64_t bluetoothAddress);

        void ReactiveBleWindowsPlugin::SendConnectionUpdate(std::string address, DeviceConnectionState state);

        std::unique_ptr<flutter::EventSink<EncodableValue>> connected_device_sink_;
        std::map<uint64_t, std::unique_ptr<BluetoothDeviceAgent>> connectedDevices{};
    };


    void ReactiveBleWindowsPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarWindows *registrar)
    {
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

        auto statusChannel =
            std::make_unique<flutter::EventChannel<EncodableValue>>(
                registrar->messenger(), "flutter_reactive_ble_status",
                &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<ReactiveBleWindowsPlugin>(registrar);

        methodChannel->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto &call, auto result)
            {
                plugin_pointer->HandleMethodCall(call, std::move(result));
            });

        auto handler = std::make_unique<
            flutter::StreamHandlerFunctions<>>(
            [plugin_pointer = plugin.get()](
                const EncodableValue *arguments,
                std::unique_ptr<flutter::EventSink<>> &&events)
                -> std::unique_ptr<flutter::StreamHandlerError<>> {
                return plugin_pointer->OnListen(arguments, std::move(events));
            },
            [plugin_pointer = plugin.get()](const EncodableValue *arguments)
                -> std::unique_ptr<flutter::StreamHandlerError<>> {
                return plugin_pointer->OnCancel(arguments);
            });

        auto charHandler = std::make_unique<flutter::BleCharHandler>();
        auto statusHandler = std::make_unique<flutter::BleStatusHandler>();

        connectedChannel->SetStreamHandler(std::move(handler));
        characteristicChannel->SetStreamHandler(std::move(charHandler));
        statusChannel->SetStreamHandler(std::move(statusHandler));

        registrar->AddPlugin(std::move(plugin));
    }


    ReactiveBleWindowsPlugin::ReactiveBleWindowsPlugin(flutter::PluginRegistrarWindows *registrar)
    {
        auto scanChannel =
            std::make_unique<flutter::EventChannel<EncodableValue>>(
                registrar->messenger(), "flutter_reactive_ble_scan",
                &flutter::StandardMethodCodec::GetInstance());
        std::unique_ptr<flutter::StreamHandler<flutter::EncodableValue>> scanHandler = std::make_unique<flutter::BleScanHandler>();
        scanChannel->SetStreamHandler(std::move(scanHandler));
    }


    ReactiveBleWindowsPlugin::~ReactiveBleWindowsPlugin() {}


    void ReactiveBleWindowsPlugin::HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue> &method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        std::string methodName = method_call.method_name();
        if (methodName.compare("initialize") == 0)
        { //TODO: Is anything needed in initialize, deinitialize, and scanForDevices now?
            result->Success();
        }
        else if (methodName.compare("deinitialize") == 0)
        {
            result->Success();
        }
        else if (methodName.compare("scanForDevices") == 0)
        {
            //TODO: Use scan parameters (List<Uuid> withServices, ScanMode scanMode, bool requireLocationServicesEnabled)?
            result->Success();
        }
        else if (methodName.compare("connectToDevice") == 0)
        { //TODO: Reduce duplication by creating a method to obtain uint64_t addr from method_call + result (?)
            // std::get_if returns a pointer to the value stored or a null pointer on error.
            // This ensures we return early if we get a null pointer.
            const flutter::EncodableValue *pEncodableValue = method_call.arguments();
            const std::vector<uint8_t> *pVector = std::get_if<std::vector<uint8_t>>(pEncodableValue);
            if (pVector && pVector->size() > 0)
            {
                ConnectToDeviceRequest req;
                // Parse vector into a protobuf message. Note the call to `pVector->data()` (https://en.cppreference.com/w/cpp/container/vector/data),
                // and note further that this may return a null pointer if pVector->size() is 0.
                bool res = req.ParsePartialFromArray(pVector->data(), pVector->size());
                if (res)
                {
                    uint64_t addr = std::stoull(req.deviceid());
                    ConnectAsync(addr);
                }
                else
                {
                    result->Error("Unable to parse message");
                    return;
                }
            }
            else
            {
                result->Error("No data");
                return;
            }
            result->Success();
        }
        else if (methodName.compare("disconnectFromDevice") == 0)
        {
            const flutter::EncodableValue *pEncodableValue = method_call.arguments();
            const std::vector<uint8_t> *pVector = std::get_if<std::vector<uint8_t>>(pEncodableValue);
            if (pVector && pVector->size() > 0)
            {
                DisconnectFromDeviceRequest req;
                bool res = req.ParsePartialFromArray(pVector->data(), pVector->size());
                if (res)
                {
                    uint64_t addr = std::stoull(req.deviceid());
                    CleanConnection(addr);
                }
                else
                {
                    result->Error("Unable to parse message");
                    return;
                }
            }
            else
            {
                result->Error("No data");
                return;
            }
            result->Success();
        }
        else if (methodName.compare("readCharacteristic") == 0)
        {
            result->NotImplemented();
        }
        else if (methodName.compare("writeCharacteristicWithResponse") == 0)  // Async data
        {
            result->NotImplemented();
        }
        else if (methodName.compare("writeCharacteristicWithoutResponse") == 0)  // Async data
        {
            result->NotImplemented();
        }
        else if (methodName.compare("readNotifications") == 0)
        {
            result->NotImplemented();
        }
        else if (methodName.compare("stopNotifications") == 0)
        {
            result->NotImplemented();
        }
        else if (methodName.compare("negotiateMtuSize") == 0)  // Async data
        {
            result->NotImplemented();
        }
        else if (methodName.compare("requestConnectionPriority") == 0)  // data
        {
            result->NotImplemented();
        }
        else if (methodName.compare("clearGattCache") == 0)  // data
        {
            result->NotImplemented();
        }
        else if (methodName.compare("discoverServices") == 0)
        {
            const flutter::EncodableValue *pEncodableValue = method_call.arguments();
            const std::vector<uint8_t> *pVector = std::get_if<std::vector<uint8_t>>(pEncodableValue);
            if (pVector && pVector->size() > 0)
            {
                DiscoverServicesRequest req;
                bool res = req.ParseFromArray(pVector->data(), pVector->size());
                if (res)
                {
                    uint64_t addr = std::stoull(req.deviceid());
                    auto iter = connectedDevices.find(addr);
                    if (iter == connectedDevices.end())
                    {
                        result->Error("Not currently connected to selected device");
                        return;
                    }
                    else
                    {
                        auto task { DiscoverServicesAsync(*iter->second, addr) };
                        std::list<DiscoveredService> data = task.get();

                        DiscoverServicesInfo info;
                        info.set_deviceid(req.deviceid());
                        for (DiscoveredService service : data)
                        {
                            info.add_services()->CopyFrom(service);
                        }
                        info.PrintDebugString();
                        
                        size_t size = info.ByteSizeLong();
                        uint8_t *buffer = (uint8_t *)malloc(size);
                        bool success = info.SerializeToArray(buffer, size);
                        if (!success)
                        {
                            std::cout << "Failed to serialize message into buffer." << std::endl;  // Debugging
                            free(buffer);
                            result->Error("Failed to serialize message into buffer.");
                        }

                        EncodableList encoded;
                        for (uint32_t i = 0; i < size; i++)
                        {
                            uint8_t value = buffer[i];
                            EncodableValue encodedVal = (EncodableValue)value;
                            encoded.push_back(encodedVal);
                        }
                        free(buffer);
                        result->Success(encoded);
                        return;
                    }
                }
                else
                {
                    result->Error("Unable to parse message");
                    return;
                }
            }
            else
            {
                result->Error("No data");
                return;
            }
        }
        else
        {
            std::cout << "Unknown method: " << methodName << std::endl; // Debugging
            result->NotImplemented();
        }
    }


    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnListenInternal(
        const EncodableValue *arguments, std::unique_ptr<flutter::EventSink<EncodableValue>> &&events)
    {
        connected_device_sink_ = std::move(events);
        return nullptr;
    }


    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> ReactiveBleWindowsPlugin::OnCancelInternal(
        const EncodableValue *arguments)
    {
        connected_device_sink_ = nullptr;
        return nullptr;
    }


    winrt::fire_and_forget ReactiveBleWindowsPlugin::ConnectAsync(uint64_t addr)
    {
        auto device = co_await BluetoothLEDevice::FromBluetoothAddressAsync(addr);
        if (!device)
        {
            OutputDebugString((L"FromBluetoothAddressAsync error: Could not find device identified by " + winrt::to_hstring(addr) + L"\n").c_str());
            SendConnectionUpdate(std::to_string(addr), DeviceConnectionState::disconnected);
            co_return;
        }
        auto servicesResult = co_await device.GetGattServicesAsync();
        if (servicesResult.Status() != GattCommunicationStatus::Success)
        {
            OutputDebugString((L"GetGattServicesAsync error: " + winrt::to_hstring((int32_t)servicesResult.Status()) + L"\n").c_str());
            SendConnectionUpdate(std::to_string(addr), DeviceConnectionState::disconnected);
            co_return;
        }
        auto connnectionStatusChangedToken = device.ConnectionStatusChanged({this, &ReactiveBleWindowsPlugin::BluetoothLEDevice_ConnectionStatusChanged});
        auto deviceAgent = std::make_unique<BluetoothDeviceAgent>(device, connnectionStatusChangedToken);
        auto pair = std::make_pair(addr, std::move(deviceAgent));
        connectedDevices.insert(std::move(pair));
        SendConnectionUpdate(std::to_string(addr), DeviceConnectionState::connected);
    }


    concurrency::task<std::list<DiscoveredService>> ReactiveBleWindowsPlugin::DiscoverServicesAsync(BluetoothDeviceAgent &bluetoothDeviceAgent, uint64_t addr)
    {
        return concurrency::create_task([&]
        {
            auto servicesResult = bluetoothDeviceAgent.device.GetGattServicesAsync().get();
            std::list<DiscoveredService> result;
            if (servicesResult.Status() != GattCommunicationStatus::Success)
            {
                OutputDebugString((L"GetGattServicesAsync error: " + winrt::to_hstring((int32_t)servicesResult.Status()) + L"\n").c_str());
                //TODO: Handle failure somewhere?
                return result;
            }
            IVectorView<GattDeviceService> services = servicesResult.Services();
            DiscoveredService converted;

            for (size_t i = 0; i < services.Size(); i++)
            {
                GattDeviceService const service = services.GetAt(i);
                converted.mutable_serviceuuid()->set_data(to_uuidstr(service.Uuid()));

                winrt::Windows::Foundation::Collections::IVectorView includedServices = service.GetIncludedServicesAsync().get().Services();
                converted.add_includedservices();
                for (size_t j = 0; j < includedServices.Size(); j++)            
                {
                    DiscoveredService tmp;
                    tmp.mutable_serviceuuid()->set_data(to_uuidstr(includedServices.GetAt(j).Uuid()));
                    converted.add_includedservices()->CopyFrom(tmp);
                }

                winrt::Windows::Foundation::Collections::IVectorView characteristics = service.GetCharacteristicsAsync().get().Characteristics();
                for (size_t j = 0; j < characteristics.Size(); j++)            
                {
                    GattCharacteristic tmp_char = characteristics.GetAt(j);
                    GattCharacteristicProperties props = tmp_char.CharacteristicProperties();
                    DiscoveredCharacteristic tmp;
                    
                    tmp.mutable_characteristicid()->set_data(to_uuidstr(tmp_char.Uuid()));
                    tmp.mutable_serviceid()->set_data(to_uuidstr(service.Uuid()));

                    tmp.set_isreadable((props & GattCharacteristicProperties::Read) != GattCharacteristicProperties::None);
                    tmp.set_iswritablewithresponse((props & GattCharacteristicProperties::Write) != GattCharacteristicProperties::None);
                    tmp.set_iswritablewithoutresponse((props & GattCharacteristicProperties::WriteWithoutResponse) != GattCharacteristicProperties::None);
                    tmp.set_isnotifiable((props & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None);
                    tmp.set_isindicatable((props & GattCharacteristicProperties::Indicate) != GattCharacteristicProperties::None);

                    converted.add_characteristics()->CopyFrom(tmp);
                    converted.add_characteristicuuids()->CopyFrom(tmp.characteristicid());
                }

                result.push_back(converted);
            }
            return result;
        });
    }


    void ReactiveBleWindowsPlugin::BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args)
    {
        OutputDebugString((L"ConnectionStatusChanged " + winrt::to_hstring((int32_t)sender.ConnectionStatus()) + L"\n").c_str());
        if (sender.ConnectionStatus() == BluetoothConnectionStatus::Disconnected)
        {
            CleanConnection(sender.BluetoothAddress());
            SendConnectionUpdate(std::to_string(sender.BluetoothAddress()), DeviceConnectionState::disconnected);
        }
    }


    void ReactiveBleWindowsPlugin::CleanConnection(uint64_t bluetoothAddress)
    {
        auto node = connectedDevices.extract(bluetoothAddress);
        if (!node.empty())
        {
            auto deviceAgent = std::move(node.mapped());
            deviceAgent->device.ConnectionStatusChanged(deviceAgent->connnectionStatusChangedToken);
            for (auto &tokenPair : deviceAgent->valueChangedTokens)
            {
                deviceAgent->gattCharacteristics.at(tokenPair.first).ValueChanged(tokenPair.second);
            }
        }
    }


    void ReactiveBleWindowsPlugin::SendConnectionUpdate(std::string address, DeviceConnectionState state)
    {
        DeviceInfo info;
        info.set_id(address);
        info.set_connectionstate(state);

        size_t size = info.ByteSizeLong();
        uint8_t *buffer = (uint8_t *)malloc(size);
        bool success = info.SerializeToArray(buffer, size);
        if (!success)
        {
            std::cout << "Failed to serialize message into buffer." << std::endl; // Debugging
            free(buffer);
            return;
        }

        EncodableList result;
        for (uint32_t i = 0; i < size; i++)
        {
            uint8_t value = buffer[i];
            EncodableValue encodedVal = (EncodableValue)value;
            result.push_back(encodedVal);
        }
        connected_device_sink_->EventSink::Success(result);
        free(buffer);
    }

} // namespace


void ReactiveBleWindowsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
    ReactiveBleWindowsPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
