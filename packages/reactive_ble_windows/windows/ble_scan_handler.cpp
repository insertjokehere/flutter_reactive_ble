#include "include/reactive_ble_windows/ble_scan_handler.h"


namespace flutter
{

    BleScanHandler::BleScanHandler(std::vector<winrt::hstring>* params) {
        auto adapter_opt = BleUtils::getAdapter();
        if (!adapter_opt.has_value()) {
            // TODO - what happens if there is no adapter on the system>
        }
        adapter = adapter_opt.value();
        adapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
            onScanFoundCallback(peripheral);
        });

        adapter.set_callback_on_scan_updated([this](SimpleBLE::Peripheral peripheral) {
            onScanFoundCallback(peripheral);
        });
    }

    /**
     * @brief Handler for OnListen to BLE scanning for devices.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @param events Unique pointer to the event sink for BLE advertisements.
     * @return std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleScanHandler::OnListenInternal(
        const EncodableValue *arguments, std::unique_ptr<flutter::EventSink<EncodableValue>> &&events)
    {
        scan_result_sink_ = std::move(events);
        std::cout << "Started scanning" << std::endl;
        adapter.scan_start();
        return nullptr;
    }


    /**
     * @brief Handler for cancelling scanning for BLE devices.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @return std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> BleScanHandler::OnCancelInternal(
        const EncodableValue *arguments)
    {
        scan_result_sink_ = nullptr;
        adapter.scan_stop();
        std::cout << "Stopped scanning" << std::endl;
        return nullptr;
    }

    void BleScanHandler::onScanFoundCallback(SimpleBLE::Peripheral peripheral) {
        std::cout << "Found device: " << peripheral.identifier() << " [" << peripheral.address() << "] "
                  << peripheral.rssi() << " dBm" << std::endl;

        if (scan_result_sink_)
        {
            DeviceScanInfo info;
            info.set_id(peripheral.address());
            info.set_name(peripheral.identifier());
            info.add_serviceuuids();
            info.add_servicedata();
            // TODO: Manufacturing data
            info.set_manufacturerdata("");
            info.set_rssi(peripheral.rssi());

            SendDeviceScanInfo(info);
        }
    }

    void BleScanHandler::onScanUpdatedCallback(SimpleBLE::Peripheral peripheral) {
        std::cout << "Updated device: " << peripheral.identifier() << " [" << peripheral.address() << "] "
                  << peripheral.rssi() << " dBm" << std::endl;
    }

    /**
     * @brief Sends the info obtained from a BLE advertisement to the scan results channel.
     * 
     * @param msg The info of the scanned device.
     */
    void BleScanHandler::SendDeviceScanInfo(DeviceScanInfo msg)
    {
        size_t size = msg.ByteSizeLong();
        uint8_t *buffer = (uint8_t *)malloc(size);
        bool success = msg.SerializeToArray(buffer, size);
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
        scan_result_sink_->EventSink::Success(result);
        free(buffer);
    }

} // namespace flutter
