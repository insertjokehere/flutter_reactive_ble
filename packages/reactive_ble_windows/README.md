# reactive_ble_windows

A new flutter plugin project.

## Getting Started

This project is a starting point for a Flutter
[plug-in package](https://flutter.dev/developing-packages/),
a specialized package that includes platform-specific implementation code for
Android and/or iOS.

For help getting started with Flutter, view our
[online documentation](https://flutter.dev/docs), which offers tutorials,
samples, guidance on mobile development, and a full API reference.

## Building
[missing - install flutter tooling and visual studio]
You will need `cmake` and `vcpkg` installed on your machine.

### cmake
Download the appropriate installer for your platform from https://cmake.org/download/ or your package manager and install `cmake`.

### vcpkg
Follow the instructions at https://github.com/microsoft/vcpkg to install `vcpkg`.

When running the `integrate install` command, take note of the path to your toolchain file.

### Set up your environment
Ensure that both the `cmake` and `vcpkg` commands are in your PATH and can be found.

Ensure that the `CMAKE_TOOLCHAIN_FILE` is set to the path you noted when installing `vcpkg`.

### Building
You can now run `flutter run -d "windows"`.

**IMPORTANT**:
You may get an error similar to this on first build:

> Error waiting for a debug connection: The log reader stopped unexpectedly, or never started.

Simply run `flutter run -d "windows"` again, and it should work.