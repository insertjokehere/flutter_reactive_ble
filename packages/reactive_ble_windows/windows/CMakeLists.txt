cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED ENV{CMAKE_TOOLCHAIN_FILE})
  message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE environment variable must be set, your build may fail.")
else()
  set(VCPKG_FEATURE_FLAGS versions)
endif()

set(PROJECT_NAME "reactive_ble_windows")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "reactive_ble_windows_plugin")

message("Windows SDK version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION, "10.0.22000.0")

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER "../protos/bledata.proto")
ADD_LIBRARY(proto ${PROTO_HEADER} ${PROTO_SRC})
include_directories(${PROTO_HEADER})

add_library(${PLUGIN_NAME} SHARED
  "reactive_ble_windows_plugin.cpp" "ble_status_handler.cpp" "ble_char_handler.cpp" "ble_scan_handler.cpp" "bluetooth_device_agent.cpp" "ble_utils.cpp"
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin ${PROTOBUF_LIBRARY} proto)


# List of absolute paths to libraries that should be bundled with the plugin
set(reactive_ble_windows_bundled_libraries
  ""
  PARENT_SCOPE
)
