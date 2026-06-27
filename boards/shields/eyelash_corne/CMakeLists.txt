# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT
#
# Top-level CMakeLists.txt for the eyelash_corne shield.
#
# ZMK automatically picks up this file when building for this shield.
# It pulls in the led_status sub-module.

cmake_minimum_required(VERSION 3.20)

# Include the LED status sub-module
include(${CMAKE_CURRENT_LIST_DIR}/led_status/CMakeLists.txt)
