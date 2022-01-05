#include "include/reactive_ble_windows/ble_status_handler.h"

#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace flutter
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Devices::Radios;
    using namespace winrt::Windows::Devices::Bluetooth;


    /**
     * @brief Converts a winrt RadioState to BleStatus enum.
     * 
     * Mapping:
     *   RadioState  |  BleStatus
     *   ------------|-------------
     *   Unknown     |  Unknown
     *   On          |  Ready
     *   Off         |  Powered Off
     *   Disabled    |  Unsupported
     * If the RadioState is not one of the 4 aforementioned types with a
     * mapping (in the event the enum changes on the Flutter side) the
     * value will be mapped to Unknown.
     * 
     * @param state The RadioState to be converted.
     * @return BleStatusInfo The nearest BleStatusInfo equivalent to the input RadioState.
     */
    BleStatusInfo RadioStateToBleStatus(RadioState state)
    {
        BleStatusInfo status;
        switch (state)
        {
        case RadioState::Unknown:
            status.set_status((BleStatus)unknown);
            break;
        case RadioState::On:
            status.set_status((BleStatus)ready);
            break;
        case RadioState::Off:
            status.set_status((BleStatus)poweredOff);
            break;
        case RadioState::Disabled:
            status.set_status((BleStatus)unsupported);
            break;
        default:
            // The default case should not be reached, unless the
            // RadioState enum on the Flutter side gets changed.
            status.set_status((BleStatus)unknown);
            break;
        }
        return status;
    }


    /**
     * @brief Handler for status change events from the Bluetooth radio.
     * 
     * @param sender The Bluetooth radio of the host machine.
     * @param args Currently unused parameter required by interface.
     */
    void BleStatusHandler::BleStatusChangedHandler(winrt::Windows::Devices::Radios::Radio const &sender, winrt::Windows::Foundation::IInspectable const &args)
    {
        RadioState state = sender.State();
        BleStatusInfo status = RadioStateToBleStatus(state);
        SendBleStatus(status);
    }


    /**
     * @brief Handler for OnListen to the host machine's Bluetooth status.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @param events Unique pointer to the event sink for Bluetooth status.
     * @return std::unique_ptr<StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<StreamHandlerError<EncodableValue>> BleStatusHandler::OnListenInternal(
        const EncodableValue *arguments,
        std::unique_ptr<EventSink<EncodableValue>> &&events)
    {
        status_result_sink_ = std::move(events);
        InitializeBleAsync();
        return nullptr;
    }


    /**
     * @brief Handler for cancelling listening to Bluetooth status.
     * 
     * @param arguments Currently unused parameter required by interface.
     * @return std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> 
     */
    std::unique_ptr<StreamHandlerError<EncodableValue>> BleStatusHandler::OnCancelInternal(
        const EncodableValue *arguments)
    {
        status_result_sink_ = nullptr;
        return nullptr;
    }


    /**
     * @brief Asynchronously initialize listening to host machine's Bluetooth status.
     * 
     * @return winrt::fire_and_forget
     */
    winrt::fire_and_forget BleStatusHandler::InitializeBleAsync()
    {
        IAsyncOperation<BluetoothAdapter> aync_op = BluetoothAdapter::GetDefaultAsync();
        BluetoothAdapter bluetoothAdapter = co_await aync_op;
        bluetoothRadio = co_await bluetoothAdapter.GetRadioAsync();
        bluetoothRadio.StateChanged({this, &BleStatusHandler::BleStatusChangedHandler});
        BleStatusInfo state = RadioStateToBleStatus(bluetoothRadio.State());
        SendBleStatus(state);
    }


    /**
     * @brief Sends an update on the Bluetooth status of the host machine to the corresponding channel.
     * 
     * @param msg The Bluetooth status of the host machine.
     */
    void BleStatusHandler::SendBleStatus(BleStatusInfo msg)
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
        status_result_sink_->EventSink::Success(result);
        free(buffer);
    }

} // namespace flutter
