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

#include <bledata.pb.h>

namespace flutter
{
    class EncodableValue;


    class BleScanHandler : public StreamHandler<EncodableValue>
    {
    public:
        BleScanHandler() {}
        virtual ~BleScanHandler() = default;

        BleScanHandler(std::vector<winrt::hstring>* params)
        {
            scanParams = params;
        }

        // Prevent copying.
        BleScanHandler(BleScanHandler const &) = delete;
        BleScanHandler &operator=(BleScanHandler const &) = delete;

    protected:
        virtual std::unique_ptr<flutter::StreamHandlerError<>> OnListenInternal(
            const EncodableValue *arguments,
            std::unique_ptr<flutter::EventSink<>> &&events) override;

        virtual std::unique_ptr<flutter::StreamHandlerError<>> OnCancelInternal(
            const EncodableValue *arguments) override;

        void DeviceWatcher_Added(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo);

        void DeviceWatcher_Updated(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate);

        void DeviceWatcher_Removed(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate);

        void DeviceWatcher_Stopped(
            winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
            winrt::Windows::Foundation::IInspectable const&);

        void SendDeviceScanInfo(DeviceScanInfo msg);

        bool initialized = false;
        std::vector<winrt::hstring>* scanParams;
        std::unique_ptr<flutter::EventSink<EncodableValue>> scan_result_sink_;
        winrt::Windows::Devices::Enumeration::DeviceWatcher deviceWatcher = nullptr;
        winrt::event_token deviceWatcherAddedToken;
        winrt::event_token deviceWatcherUpdatedToken;
        winrt::event_token deviceWatcherRemovedToken;
        winrt::event_token deviceWatcherStoppedToken;
        std::map<std::string, DeviceScanInfo> discoveredDevices;
    };

} // namespace flutter

#endif // BLE_SCAN_HANDLER_H
