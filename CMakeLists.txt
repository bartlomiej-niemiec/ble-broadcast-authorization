cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/components" "${CMAKE_SOURCE_DIR}/projects")

# Include the ESP-IDF project build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(esp32_ble_broadcast_authentication)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
