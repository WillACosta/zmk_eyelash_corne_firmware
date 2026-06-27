/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * LED Status System – Public API
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Flash the profile LED blue for LED_PROFILE_DISPLAY_MS.
 *
 * Called automatically on zmk_ble_active_profile_changed, but also
 * available for explicit invocation.
 *
 * @param profile  Zero-based profile index (0–4).
 */
void led_status_show_profile(uint8_t profile);

/**
 * @brief Run the pairing blink animation on the profile LED.
 *
 * Blinks the corresponding profile LED blue (LED_PAIRING_CYCLES times),
 * then turns it off.
 *
 * @param profile  Zero-based profile index (0–4).
 */
void led_status_show_pairing(uint8_t profile);

/**
 * @brief Show host-connection status on the V key LED for LED_STATUS_DISPLAY_MS.
 *
 * @param connected  true → green, false → red.
 */
void led_status_show_host(bool connected);

/**
 * @brief Show split-connection status on the B key LED for LED_STATUS_DISPLAY_MS.
 *
 * @param connected  true → green, false → red.
 */
void led_status_show_split(bool connected);

/**
 * @brief Show battery level as a bar-graph on Q/A/Z LEDs for LED_STATUS_DISPLAY_MS.
 *
 * @param percentage  Current state-of-charge (0–100).
 */
void led_status_show_battery(uint8_t percentage);
