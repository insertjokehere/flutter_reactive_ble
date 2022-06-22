# reactive_ble_windows

A Flutter plugin to enable Bluetooth Low Energy on Windows, intended for use in conjunction with flutter_reactive_ble.

## Building
[missing - install flutter tooling and visual studio]
You will need `vcpkg` installed on your machine.

### vcpkg
Follow the instructions at https://github.com/microsoft/vcpkg to install `vcpkg`.

When running the `integrate install` command, take note of the path to your toolchain file.

### Set up your environment
Ensure that `vcpkg` is in your PATH and can be found.

Ensure that the `CMAKE_TOOLCHAIN_FILE` is set to the path you noted when installing `vcpkg`.

### Building

For projects that use this library, copy `windows/vcpkg.json` into the `windows/` directory of your project to ensure that this libraries dependencies get built automatically.

You can now run `flutter run -d "windows"`.

**IMPORTANT**:
You may get an error similar to this on first build:

> Error waiting for a debug connection: The log reader stopped unexpectedly, or never started.

Simply run `flutter run -d "windows"` again, and it should work.