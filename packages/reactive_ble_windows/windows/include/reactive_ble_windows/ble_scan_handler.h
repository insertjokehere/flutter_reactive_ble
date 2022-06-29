#ifndef BLE_SCAN_HANDLER_H
#define BLE_SCAN_HANDLER_H

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler.h>
#include <flutter/standard_method_codec.h>

#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Devices.Radios.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/base.h"

#include <simpleble/Types.h>

#include <map>
#include <sstream>

#include <bledata.pb.h>
#include "ble_utils.h"

namespace flutter
{
    class BleScanHandler : public StreamHandler<EncodableValue>
    {
    public:
        BleScanHandler(std::vector<winrt::hstring>* params);
        virtual ~BleScanHandler() = default;

        // Prevent copying.
        BleScanHandler(BleScanHandler const &) = delete; 
        BleScanHandler &operator=(BleScanHandler const &) = delete;

    private:
        SimpleBLE::Adapter adapter;
        std::unique_ptr<flutter::EventSink<EncodableValue>> scan_result_sink_;

    protected:
        virtual std::unique_ptr<flutter::StreamHandlerError<>> OnListenInternal(
            const EncodableValue *arguments,
            std::unique_ptr<flutter::EventSink<>> &&events) override;

        virtual std::unique_ptr<flutter::StreamHandlerError<>> OnCancelInternal(
            const EncodableValue *arguments) override;

        void SendDeviceScanInfo(DeviceScanInfo msg);
        void onScanFoundCallback(SimpleBLE::Peripheral peripheral);
        void onScanUpdatedCallback(SimpleBLE::Peripheral peripheral);
    };

} // namespace flutter

#endif // BLE_SCAN_HANDLER_H
