cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME "reactive_ble_windows")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "reactive_ble_windows_plugin")

add_library(${PLUGIN_NAME} SHARED
  "reactive_ble_windows_plugin.cpp"
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

SET(PROTO_DIR "C:/Users/tobia/Projects/flutter_reactive_ble/packages/reactive_ble_windows/lib/src/generated")
ADD_LIBRARY(proto "${PROTO_DIR}/bledata.pb.h" "${PROTO_DIR}/bledata.pb.cc")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite proto)

# include_directories("C:/Users/JuliaHarrison/Repos/vcpkg/testapp/vcpkg_installed/x86-windows/include")

# List of absolute paths to libraries that should be bundled with the plugin
set(reactive_ble_windows_bundled_libraries
  ""
  PARENT_SCOPE
)
