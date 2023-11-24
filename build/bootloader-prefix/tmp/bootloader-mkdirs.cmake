# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/liwka/esp/esp-idf/components/bootloader/subproject"
  "C:/Projetos/ota_uart/build/bootloader"
  "C:/Projetos/ota_uart/build/bootloader-prefix"
  "C:/Projetos/ota_uart/build/bootloader-prefix/tmp"
  "C:/Projetos/ota_uart/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Projetos/ota_uart/build/bootloader-prefix/src"
  "C:/Projetos/ota_uart/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Projetos/ota_uart/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
