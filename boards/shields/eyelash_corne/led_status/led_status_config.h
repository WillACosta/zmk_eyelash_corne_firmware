/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * LED Status System – Configuration
 *
 * This file defines the LED indices for the left-half (central) status
 * indicators.  All indices are 0-based and refer to the ws2812 chain-length
 * array exposed by ZMK's underglow driver.
 *
 * Eyelash Corne left-half physical LED layout (chain-length = 21):
 *
 *   Row 0:  0   1   2   3   4   5   6
 *           │   Q   W   E   R   T   │
 *   Row 1:  7   8   9  10  11  12  13
 *          Esc  A   S   D   F   G   │
 *   Row 2: 14  15  16  17  18  19  20
 *           │   Z   X   C   V   B   │
 *
 * Thumbs are wired after the three main rows in this firmware; if your
 * keyboard differs, adjust these values accordingly.
 *
 * Key → LED mapping (col-0 is the extra outer column, LEDs are in physical
 * order left-to-right inside each row):
 *
 *   Q = 1   W = 2   E = 3   R = 4   T = 5
 *   A = 8   S = 9   D = 10  F = 11  G = 12
 *   Z = 15  X = 16  C = 17  V = 18  B = 19
 */

#pragma once

/* ------------------------------------------------------------------ */
/* Bluetooth profile LEDs  (A / S / D / F / G)                        */
/* ------------------------------------------------------------------ */
#define LED_PROFILE_0   8   /* A */
#define LED_PROFILE_1   9   /* S */
#define LED_PROFILE_2   10  /* D */
#define LED_PROFILE_3   11  /* F */
#define LED_PROFILE_4   12  /* G */

/* ------------------------------------------------------------------ */
/* Battery bar-graph LEDs  (Q / A / Z – left-most column)             */
/* ------------------------------------------------------------------ */
#define LED_BATTERY_TOP     1   /* Q */
#define LED_BATTERY_MIDDLE  8   /* A  (shared with PROFILE_0 – see spec note) */
#define LED_BATTERY_BOTTOM  15  /* Z */

/* ------------------------------------------------------------------ */
/* Connection status LEDs                                              */
/* ------------------------------------------------------------------ */
#define LED_HOST_STATUS   18  /* V */
#define LED_SPLIT_STATUS  19  /* B */

/* ------------------------------------------------------------------ */
/* Timing constants (milliseconds)                                     */
/* ------------------------------------------------------------------ */
#define LED_PROFILE_DISPLAY_MS   500
#define LED_PAIRING_ON_MS        150
#define LED_PAIRING_OFF_MS       150
#define LED_PAIRING_CYCLES       5
#define LED_STATUS_DISPLAY_MS    1000

/* ------------------------------------------------------------------ */
/* Brightness (0-255 per channel)                                      */
/* ------------------------------------------------------------------ */
#define LED_STATUS_BRIGHTNESS    80
