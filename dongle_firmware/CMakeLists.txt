# CodeBuddy Dongle Firmware
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 将 main/ 添加到全局 include 路径，让 TinyUSB 能找到 tusb_config.h
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/main" APPEND)

project(codebuddy_dongle)
