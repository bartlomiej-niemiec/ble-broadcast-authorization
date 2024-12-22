cmake_minimum_required(VERSION 3.16)

# Default to ble_receiver_app if no APP_TARGET is specified
if(NOT DEFINED APP_TARGET)
    set(APP_TARGET "receiver_app")
endif()

message(STATUS "Building APP_TARGET: ${APP_TARGET}")

# Explicitly include the custom directory as the main component
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/projects/${APP_TARGET}")

# Include the ESP-IDF project build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(esp32_ble_broadcast_authentication)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)

