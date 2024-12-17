# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/esp-idf/components/bootloader/subproject"
  "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader"
  "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix"
  "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix/tmp"
  "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix/src/bootloader-stamp"
  "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix/src"
  "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/GitHub/MyRepos/esp32-ble-broadcast-authentication/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
