#ifndef BLE_SCAN_HANDLER_H
#define BLE_SCAN_HANDLER_H

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler.h>
#include <flutter/standard_method_codec.h>

#include <windows.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <map>
#include <sstream>

#include "../lib/src/generated/bledata.pb.h"

namespace flutter
{
    class EncodableValue;


    class BleScanHandler : public StreamHandler<EncodableValue>
    {
    public:
        BleScanHandler() {}
        virtual ~BleScanHandler() = default;

        // Prevent copying.
        BleScanHandler(BleScanHandler const &) = delete;
        BleScanHandler &operator=(BleScanHandler const &) = delete;

    protected:
        virtual std::unique_ptr<flutter::StreamHandlerError<>> OnListenInternal(
            const EncodableValue *arguments,
            std::unique_ptr<flutter::EventSink<>> &&events) override;

        virtual std::unique_ptr<flutter::StreamHandlerError<>> OnCancelInternal(
            const EncodableValue *arguments) override;

        // void OnAdvertisementReceived(
        //     winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher watcher,
        //     winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs args);
        void DeviceWatcher_Added(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo);

        void DeviceWatcher_Updated(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate);

        void DeviceWatcher_Removed(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate);

        void DeviceWatcher_EnumerationCompleted(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Foundation::IInspectable const&);

        void DeviceWatcher_Stopped(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Foundation::IInspectable const&);

        void SendDeviceScanInfo(DeviceScanInfo msg);

        bool initialized = false;
        // winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher bleWatcher = nullptr;
        // winrt::event_token bluetoothLEWatcherReceivedToken;
        std::unique_ptr<flutter::EventSink<EncodableValue>> scan_result_sink_;
        winrt::Windows::Devices::Enumeration::DeviceWatcher deviceWatcher = nullptr;
        winrt::event_token deviceWatcherAddedToken;
        winrt::event_token deviceWatcherUpdatedToken;
        winrt::event_token deviceWatcherRemovedToken;
        winrt::event_token deviceWatcherEnumerationCompletedToken;
        winrt::event_token deviceWatcherStoppedToken;
        std::map<std::string, DeviceScanInfo> discoveredDevices;
    };

} // namespace flutter

#endif // BLE_SCAN_HANDLER_H
