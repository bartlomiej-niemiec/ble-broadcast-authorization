cmake_minimum_required(VERSION 3.16)

# Include the ESP-IDF project build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Use the selected APP_TARGET from sdkconfig
idf_build_get_property(CONFIG_APP_TARGET CONFIG_APP_TARGET)

if(NOT CONFIG_APP_TARGET)
    # Dynamically set the EXTRA_COMPONENT_DIRS based on the selected APP_TARGET
    set(CONFIG_APP_TARGET "receiver_app")
    message(STATUS "APP_TARGET is not defined. Please configure it in sdkconfig.")
endif()

set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/projects/${CONFIG_APP_TARGET}")

message(STATUS "Building APP_TARGET: ${CONFIG_APP_TARGET}")

# Project name
project(esp32_ble_broadcast_authentication)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
