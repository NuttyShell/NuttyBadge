# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(NuttyOS)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/ussr.bin" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/ussr_8k.bin" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/ussr_48k.bin" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/ussr_96k.bin" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/images/boot_icon.png" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/images/about.png" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/audio/chipi_mono8b48k.bin" BINARY)
