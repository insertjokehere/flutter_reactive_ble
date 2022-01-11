import 'dart:async';

import 'package:flutter/services.dart';
import 'package:reactive_ble_platform_interface/reactive_ble_platform_interface.dart';
import 'converter/args_to_protubuf_converter.dart';
import 'converter/protobuf_converter.dart';

class ReactiveBleWindowsPlatform extends ReactiveBlePlatform {
  ReactiveBleWindowsPlatform({
    required ArgsToProtobufConverter argsToProtobufConverter,
    required ProtobufConverter protobufConverter,
    required MethodChannel bleMethodChannel,
    required Stream<List<int>> bleStatusChannel,
    required Stream<List<int>> connectedDeviceChannel,
    required Stream<List<int>> charUpdateChannel,
    required Stream<List<int>> bleDeviceScanChannel,
  })  : _argsToProtobufConverter = argsToProtobufConverter,
        _protobufConverter = protobufConverter,
        _bleMethodChannel = bleMethodChannel,
        _bleStatusRawChannel = bleStatusChannel,
        _connectedDeviceRawStream = connectedDeviceChannel,
        _charUpdateRawStream = charUpdateChannel,
        _bleDeviceScanRawStream = bleDeviceScanChannel;

  final ArgsToProtobufConverter _argsToProtobufConverter;
  final ProtobufConverter _protobufConverter;
  final MethodChannel _bleMethodChannel;
  final Stream<List<int>> _connectedDeviceRawStream;
  final Stream<List<int>> _charUpdateRawStream;
  final Stream<List<int>> _bleDeviceScanRawStream;
  final Stream<List<int>> _bleStatusRawChannel;

  Stream<ConnectionStateUpdate>? _connectionUpdateStream;
  Stream<CharacteristicValue>? _charValueStream;
  Stream<ScanResult>? _scanResultStream;
  Stream<BleStatus>? _bleStatusStream;

  @override
  Stream<ConnectionStateUpdate> get connectionUpdateStream =>
      _connectionUpdateStream ??= _connectedDeviceRawStream
          .map(_protobufConverter.connectionStateUpdateFrom)
          .map(
            (update) => update,
          );

  @override
  Stream<CharacteristicValue> get charValueUpdateStream => _charValueStream ??=
      _charUpdateRawStream.map(_protobufConverter.characteristicValueFrom).map(
            (update) => update,
          );

  @override
  Stream<ScanResult> get scanStream => _scanResultStream ??=
      _bleDeviceScanRawStream.map(_protobufConverter.scanResultFrom).map(
            (scanResult) => scanResult,
          );

  @override
  Stream<BleStatus> get bleStatusStream =>
      _bleStatusStream ??= _bleStatusRawChannel
          .map(_protobufConverter.bleStatusFrom)
          .map((status) => status);

  @override
  Future<void> initialize() => _bleMethodChannel.invokeMethod("initialize");

  @override
  Future<void> deinitialize() =>
      _bleMethodChannel.invokeMethod<void>("deinitialize");

  @override
  Stream<void> scanForDevices({
    required List<Uuid> withServices,
    required ScanMode scanMode,
    required bool requireLocationServicesEnabled,
  }) =>
      _bleMethodChannel
          .invokeMethod<void>(
            "scanForDevices",
            _argsToProtobufConverter
                .createScanForDevicesRequest(
                  withServices: withServices,
                  scanMode: scanMode,
                  requireLocationServicesEnabled:
                      requireLocationServicesEnabled,
                )
                .writeToBuffer(),
          )
          .asStream();

  @override
  Stream<void> connectToDevice(
    String id,
    Map<Uuid, List<Uuid>>? servicesWithCharacteristicsToDiscover,
    Duration? connectionTimeout,
  ) =>
      _bleMethodChannel
          .invokeMethod<void>(
            "connectToDevice",
            _argsToProtobufConverter
                .createConnectToDeviceArgs(
                  id,
                  servicesWithCharacteristicsToDiscover,
                  connectionTimeout,
                )
                .writeToBuffer(),
          )
          .asStream();

  @override
  Future<void> disconnectDevice(String deviceId) =>
      _bleMethodChannel.invokeMethod<void>(
        "disconnectFromDevice",
        _argsToProtobufConverter
            .createDisconnectDeviceArgs(deviceId)
            .writeToBuffer(),
      );

  @override
  Stream<void> readCharacteristic(QualifiedCharacteristic characteristic) =>
      _bleMethodChannel
          .invokeMethod<void>(
            "readCharacteristic",
            _argsToProtobufConverter
                .createReadCharacteristicRequest(characteristic)
                .writeToBuffer(),
          )
          .asStream();

  @override
  Future<WriteCharacteristicInfo> writeCharacteristicWithResponse(
    QualifiedCharacteristic characteristic,
    List<int> value,
  ) async =>
      _bleMethodChannel
          .invokeMethod(
              "writeCharacteristicWithResponse",
              _argsToProtobufConverter
                  .createWriteChacracteristicRequest(characteristic, value)
                  .writeToBuffer())
          .then((data) => _protobufConverter
              .writeCharacteristicInfoFrom(List<int>.from(data!)));

  @override
  Future<WriteCharacteristicInfo> writeCharacteristicWithoutResponse(
    QualifiedCharacteristic characteristic,
    List<int> value,
  ) async =>
      _bleMethodChannel
          .invokeMethod(
            "writeCharacteristicWithoutResponse",
            _argsToProtobufConverter
                .createWriteChacracteristicRequest(characteristic, value)
                .writeToBuffer(),
          )
          .then((data) => _protobufConverter
              .writeCharacteristicInfoFrom(List<int>.from(data!)));

  @override
  Stream<void> subscribeToNotifications(
    QualifiedCharacteristic characteristic,
  ) =>
      _bleMethodChannel
          .invokeMethod<void>(
            "readNotifications",
            _argsToProtobufConverter
                .createNotifyCharacteristicRequest(characteristic)
                .writeToBuffer(),
          )
          .asStream();

  @override
  Future<void> stopSubscribingToNotifications(
    QualifiedCharacteristic characteristic,
  ) =>
      _bleMethodChannel
          .invokeMethod<void>(
            "stopNotifications",
            _argsToProtobufConverter
                .createNotifyNoMoreCharacteristicRequest(characteristic)
                .writeToBuffer(),
          )
          .catchError(
            // ignore: avoid_print
            (Object e) => print("Error unsubscribing from notifications: $e"),
          );

  @override
  Future<int> requestMtuSize(String deviceId, int? mtu) async =>
      _bleMethodChannel
          .invokeMethod<List<int>>(
            "negotiateMtuSize",
            _argsToProtobufConverter
                .createNegotiateMtuRequest(deviceId, mtu!)
                .writeToBuffer(),
          )
          .then((data) => _protobufConverter.mtuSizeFrom(data!));

  @override
  Future<ConnectionPriorityInfo> requestConnectionPriority(
          String deviceId, ConnectionPriority priority) =>
      _bleMethodChannel
          .invokeMethod<List<int>>(
            "requestConnectionPriority",
            _argsToProtobufConverter
                .createChangeConnectionPrioRequest(deviceId, priority)
                .writeToBuffer(),
          )
          .then((data) => _protobufConverter.connectionPriorityInfoFrom(data!));

  @override
  Future<Result<Unit, GenericFailure<ClearGattCacheError>?>> clearGattCache(
          String deviceId) =>
      _bleMethodChannel
          .invokeMethod<List<int>>(
            "clearGattCache",
            _argsToProtobufConverter
                .createClearGattCacheRequest(deviceId)
                .writeToBuffer(),
          )
          .then((data) => _protobufConverter.clearGattCacheResultFrom(data!));

  List<DiscoveredService> parse(data) {
    // Helper method for readability
    List<DiscoveredService> dat =
        _protobufConverter.discoveredServicesFrom(List<int>.from(data!));
    return dat;
  }

  @override
  Future<List<DiscoveredService>> discoverServices(String deviceId) async =>
      _bleMethodChannel
          .invokeMethod(
            "discoverServices",
            _argsToProtobufConverter
                .createDiscoverServicesRequest(deviceId)
                .writeToBuffer(),
          )
          .then(parse);
}

class ReactiveBleWindowsPlatformFactory {
  const ReactiveBleWindowsPlatformFactory();

  ReactiveBleWindowsPlatform create() {
    const _bleMethodChannel = MethodChannel("flutter_reactive_ble_method");
    const bleStatusChannel = EventChannel("flutter_reactive_ble_status");
    const connectedDeviceChannel =
        EventChannel("flutter_reactive_ble_connected_device");
    const charEventChannel = EventChannel("flutter_reactive_ble_char_update");
    const scanEventChannel = EventChannel("flutter_reactive_ble_scan");
    return ReactiveBleWindowsPlatform(
      protobufConverter: const ProtobufConverterImpl(),
      argsToProtobufConverter: const ArgsToProtobufConverterImpl(),
      bleMethodChannel: _bleMethodChannel,
      bleStatusChannel:
          bleStatusChannel.receiveBroadcastStream().map<List<int>>((e) {
        List<int> result = [];
        e.forEach((value) => result.add(value));
        return result;
      }),
      connectedDeviceChannel:
          connectedDeviceChannel.receiveBroadcastStream().map<List<int>>((e) {
        List<int> result = [];
        e.forEach((value) => result.add(value));
        return result;
      }),
      charUpdateChannel:
          charEventChannel.receiveBroadcastStream().map<List<int>>((e) {
        List<int> result = [];
        e.forEach((value) => result.add(value));
        return result;
      }),
      bleDeviceScanChannel:
          scanEventChannel.receiveBroadcastStream().map<List<int>>((e) {
        List<int> result = [];
        e.forEach((value) => result.add(value));
        return result;
      }),
    );
  }
}
