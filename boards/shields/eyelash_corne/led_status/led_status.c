/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * LED Status System – Implementation
 *
 * Provides visual keyboard-status feedback through the per-key RGB LEDs on
 * the left (central) half.  All LEDs are off by default; they only light up
 * during brief event-driven animations.
 *
 * Indicators
 * ----------
 *  FR-1  Bluetooth profile selection  → profile key flashes BLUE 500 ms
 *  FR-2  Bluetooth pairing mode       → profile key blinks BLUE 5×150 ms
 *  FR-3  Host connection status       → V key GREEN/RED 1000 ms
 *  FR-4  Split connection status      → B key GREEN/RED 1000 ms
 *  FR-5  Battery level                → Q/A/Z bar-graph 1000 ms
 *
 * Event subscriptions (left half / central only)
 * -----------------------------------------------
 *  zmk_ble_active_profile_changed      → FR-1 (and FR-2 when pairing)
 *  zmk_split_peripheral_status_changed → cached for FR-4
 *  zmk_battery_state_changed           → cached for FR-5
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/ble.h>
#include <zmk/battery.h>

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
#include <zmk/rgb_underglow.h>
#endif

#include "led_status.h"
#include "led_status_config.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ======================================================================
 * LED strip device
 * ====================================================================== */

#define STRIP_NODE DT_CHOSEN(zmk_underglow)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)

static const struct device *led_strip_dev;

/* Pixel buffer – shadows the full LED chain.  We write individual pixels
 * and then push the entire buffer to the strip driver. */
static struct led_rgb pixels[STRIP_NUM_PIXELS];

/* ======================================================================
 * Cached state (updated by event listeners)
 * ====================================================================== */

static bool split_connected = false;
static uint8_t battery_pct = 100;

/* ======================================================================
 * Colour helpers
 * ====================================================================== */

#define B LED_STATUS_BRIGHTNESS

static const struct led_rgb COLOR_OFF    = {.r = 0,   .g = 0,   .b = 0};
static const struct led_rgb COLOR_BLUE   = {.r = 0,   .g = 0,   .b = B};
static const struct led_rgb COLOR_GREEN  = {.r = 0,   .g = B,   .b = 0};
static const struct led_rgb COLOR_RED    = {.r = B,   .g = 0,   .b = 0};
/* Orange ≈ full red + half green */
static const struct led_rgb COLOR_ORANGE = {.r = B,   .g = B/2, .b = 0};
static const struct led_rgb COLOR_PURPLE = {.r = B,   .g = 0,   .b = B};

/* Write a single pixel and flush.  Pixels outside the left-half chain are
 * silently ignored to protect against misconfiguration. */
static void set_pixel(uint8_t idx, struct led_rgb color)
{
    if (!led_strip_dev || idx >= STRIP_NUM_PIXELS) {
        return;
    }
    pixels[idx] = color;
    led_strip_update_rgb(led_strip_dev, pixels, STRIP_NUM_PIXELS);
}

/* Set several pixels in one flush. */
static void set_pixels_and_flush(uint8_t *indices, struct led_rgb *colors, uint8_t count)
{
    if (!led_strip_dev) {
        return;
    }
    for (uint8_t i = 0; i < count; i++) {
        if (indices[i] < STRIP_NUM_PIXELS) {
            pixels[indices[i]] = colors[i];
        }
    }
    led_strip_update_rgb(led_strip_dev, pixels, STRIP_NUM_PIXELS);
}

/* ======================================================================
 * Work-queue items  (all LED ops happen from the ZMK system work queue)
 * ====================================================================== */

/* --- FR-1: Profile flash --- */
static uint8_t profile_work_idx;

static void profile_off_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(profile_off_work, profile_off_work_handler);

static void profile_on_work_handler(struct k_work *work)
{
    set_pixel(profile_work_idx, COLOR_BLUE);
}
K_WORK_DEFINE(profile_on_work, profile_on_work_handler);

static void profile_off_work_handler(struct k_work *work)
{
    set_pixel(profile_work_idx, COLOR_OFF);
    refresh_underglow_suspension();
}

/* --- FR-2: Pairing blink --- */
struct pairing_work_data {
    uint8_t led_idx;
    uint8_t remaining_cycles;
    bool phase_on; /* true = currently on, false = currently off */
};
static struct pairing_work_data pairing_data;

static void pairing_step_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(pairing_step_work, pairing_step_work_handler);

static void pairing_step_work_handler(struct k_work *work)
{
    if (pairing_data.remaining_cycles == 0) {
        set_pixel(pairing_data.led_idx, COLOR_OFF);
        refresh_underglow_suspension();
        return;
    }

    if (pairing_data.phase_on) {
        set_pixel(pairing_data.led_idx, COLOR_BLUE);
        pairing_data.phase_on = false;
        k_work_schedule(&pairing_step_work, K_MSEC(LED_PAIRING_ON_MS));
    } else {
        set_pixel(pairing_data.led_idx, COLOR_OFF);
        pairing_data.phase_on = true;
        pairing_data.remaining_cycles--;
        k_work_schedule(&pairing_step_work, K_MSEC(LED_PAIRING_OFF_MS));
    }
}

/* --- FR-3: Host status --- */
static bool host_status_connected;

static void host_off_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(host_off_work, host_off_work_handler);

static void host_on_work_handler(struct k_work *work)
{
    set_pixel(LED_HOST_STATUS,
              host_status_connected ? COLOR_PURPLE : COLOR_RED);
}
K_WORK_DEFINE(host_on_work, host_on_work_handler);

static void host_off_work_handler(struct k_work *work)
{
    set_pixel(LED_HOST_STATUS, COLOR_OFF);
    refresh_underglow_suspension();
}

/* --- FR-4: Split status --- */
static bool split_status_connected_arg;

static void split_off_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(split_off_work, split_off_work_handler);

static void split_on_work_handler(struct k_work *work)
{
    set_pixel(LED_SPLIT_STATUS,
              split_status_connected_arg ? COLOR_BLUE : COLOR_RED);
}
K_WORK_DEFINE(split_on_work, split_on_work_handler);

static void split_off_work_handler(struct k_work *work)
{
    set_pixel(LED_SPLIT_STATUS, COLOR_OFF);
    refresh_underglow_suspension();
}

/* --- FR-5: Battery --- */
static uint8_t battery_work_pct;

static void battery_off_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(battery_off_work, battery_off_work_handler);

static void battery_on_work_handler(struct k_work *work)
{
    uint8_t pct = battery_work_pct;
    struct led_rgb color;

    /* Colour by threshold */
    if (pct > 80) {
        color = COLOR_GREEN;
    } else if (pct > 40) {
        color = COLOR_ORANGE;
    } else {
        color = COLOR_RED;
    }

    /*
     * Bar-graph fill (spec table):
     *   80-100 % → ● ● ●
     *   60- 79 % → ● ● ○
     *   40- 59 % → ● ● ○  (same as above per spec; orange colour differentiates)
     *   20- 39 % → ● ○ ○
     *    0- 19 % → ● ○ ○  (same bars; red colour differentiates)
     *
     * Bottom LED (Z) always ON, middle (A) ON above 40 %, top (Q) ON above 60 %.
     */
    uint8_t indices[3] = {LED_BATTERY_BOTTOM, LED_BATTERY_MIDDLE, LED_BATTERY_TOP};
    struct led_rgb colors[3];

    colors[0] = color;                        /* bottom: always on */
    colors[1] = (pct > 40)  ? color : COLOR_OFF;
    colors[2] = (pct > 60)  ? color : COLOR_OFF;

    set_pixels_and_flush(indices, colors, 3);
}
K_WORK_DEFINE(battery_on_work, battery_on_work_handler);

static void battery_off_work_handler(struct k_work *work)
{
    uint8_t indices[3] = {LED_BATTERY_BOTTOM, LED_BATTERY_MIDDLE, LED_BATTERY_TOP};
    struct led_rgb colors[3] = {COLOR_OFF, COLOR_OFF, COLOR_OFF};
    set_pixels_and_flush(indices, colors, 3);
    refresh_underglow_suspension();
}

static bool underglow_was_on = false;
static bool suspended_by_us = false;

static void refresh_underglow_suspension(void);


/* ======================================================================
 * Public API
 * ====================================================================== */

void led_status_show_profile(uint8_t profile)
{
    const uint8_t led_map[5] = {
        LED_PROFILE_0, LED_PROFILE_1, LED_PROFILE_2,
        LED_PROFILE_3, LED_PROFILE_4
    };

    if (profile >= 5) {
        return;
    }

    /* Cancel any pending off-timer for the same LED */
    k_work_cancel_delayable(&profile_off_work);

    profile_work_idx = led_map[profile];
    k_work_submit(&profile_on_work);
    k_work_schedule(&profile_off_work, K_MSEC(LED_PROFILE_DISPLAY_MS));
    refresh_underglow_suspension();
}

void led_status_show_pairing(uint8_t profile)
{
    const uint8_t led_map[5] = {
        LED_PROFILE_0, LED_PROFILE_1, LED_PROFILE_2,
        LED_PROFILE_3, LED_PROFILE_4
    };

    if (profile >= 5) {
        return;
    }

    /* Cancel any in-progress pairing animation */
    k_work_cancel_delayable(&pairing_step_work);

    pairing_data.led_idx         = led_map[profile];
    pairing_data.remaining_cycles = LED_PAIRING_CYCLES;
    pairing_data.phase_on        = true;

    /* Kick off immediately */
    k_work_schedule(&pairing_step_work, K_NO_WAIT);
    refresh_underglow_suspension();
}

void led_status_show_host(bool connected)
{
    k_work_cancel_delayable(&host_off_work);
    host_status_connected = connected;
    k_work_submit(&host_on_work);
    k_work_schedule(&host_off_work, K_MSEC(LED_STATUS_DISPLAY_MS));
    refresh_underglow_suspension();
}

void led_status_show_split(bool connected)
{
    k_work_cancel_delayable(&split_off_work);
    split_status_connected_arg = connected;
    k_work_submit(&split_on_work);
    k_work_schedule(&split_off_work, K_MSEC(LED_STATUS_DISPLAY_MS));
    refresh_underglow_suspension();
}

void led_status_show_battery(uint8_t percentage)
{
    k_work_cancel_delayable(&battery_off_work);
    battery_work_pct = percentage;
    k_work_submit(&battery_on_work);
    k_work_schedule(&battery_off_work, K_MSEC(LED_STATUS_DISPLAY_MS));
    refresh_underglow_suspension();
}

/* ======================================================================
 * ZMK event listeners
 * ====================================================================== */

/* FR-1 / FR-2: BLE active profile changed
 *
 * ZMK raises this event both when the user switches profiles AND when the
 * profile enters/exits pairing mode.  We check zmk_ble_active_profile_is_open()
 * to distinguish pairing from a simple switch.
 */
static int ble_profile_listener(const zmk_event_t *eh)
{
    uint8_t idx = zmk_ble_active_profile_index();
    bool open = zmk_ble_active_profile_is_open();

    LOG_INF("BLE active profile changed: index=%d, open/pairing=%d", idx, open);

    if (open) {
        /* Profile is open (unbound) → pairing mode */
        led_status_show_pairing(idx);
    } else {
        /* Profile switched */
        led_status_show_profile(idx);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(led_status_ble_profile, ble_profile_listener);
ZMK_SUBSCRIPTION(led_status_ble_profile, zmk_ble_active_profile_changed);

/* FR-4: Split peripheral connection changes – cache the state.
 * The combo action calls led_status_show_split() with the cached value.
 */
static int split_status_listener(const zmk_event_t *eh)
{
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (ev) {
        LOG_INF("Split status event: connected=%d", ev->connected);
        split_connected = ev->connected;
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(led_status_split, split_status_listener);
ZMK_SUBSCRIPTION(led_status_split, zmk_split_peripheral_status_changed);

/* FR-5: Battery state – cache the latest percentage. */
static int battery_listener(const zmk_event_t *eh)
{
    const struct zmk_battery_state_changed *ev =
        as_zmk_battery_state_changed(eh);
    if (ev) {
        LOG_INF("Battery state event: percentage=%d", ev->state_of_charge);
        battery_pct = ev->state_of_charge;
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(led_status_battery, battery_listener);
ZMK_SUBSCRIPTION(led_status_battery, zmk_battery_state_changed);

/* ======================================================================
 * Combo-triggered entry points
 *
 * These thin wrappers are called from ZMK behaviors (zmk_macro or a custom
 * behavior defined in the keymap).  They forward to the public API using the
 * latest cached state.
 * ====================================================================== */

/**
 * @brief Query and show host-connection status (for use from keymap).
 */
void led_status_query_host(void)
{
    bool connected = zmk_ble_active_profile_is_connected();
    LOG_INF("Query host status: connected=%d", connected);
    led_status_show_host(connected);
}

/**
 * @brief Query and show split-connection status (for use from keymap).
 */
void led_status_query_split(void)
{
    LOG_INF("Query split status: connected=%d", split_connected);
    led_status_show_split(split_connected);
}

/**
 * @brief Query and show battery status (for use from keymap).
 */
void led_status_query_battery(void)
{
    LOG_INF("Query battery status: percentage=%d", battery_pct);
    led_status_show_battery(battery_pct);
}

/* ======================================================================
 * Initialisation
 * ====================================================================== */

static int led_status_init(void)
{
    led_strip_dev = DEVICE_DT_GET(STRIP_NODE);

    if (!device_is_ready(led_strip_dev)) {
        LOG_ERR("LED strip device not ready");
        led_strip_dev = NULL;
        return -ENODEV;
    }

    /* Ensure all LEDs start off */
    memset(pixels, 0, sizeof(pixels));
    led_strip_update_rgb(led_strip_dev, pixels, STRIP_NUM_PIXELS);

    LOG_INF("LED status system initialised (%d pixels)", STRIP_NUM_PIXELS);
    return 0;
}

SYS_INIT(led_status_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static void refresh_underglow_suspension(void)
{
#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
    bool any_active = k_work_delayable_is_pending(&profile_off_work) ||
                      k_work_delayable_is_pending(&pairing_step_work) ||
                      k_work_delayable_is_pending(&host_off_work) ||
                      k_work_delayable_is_pending(&split_off_work) ||
                      k_work_delayable_is_pending(&battery_off_work);

    if (any_active && !suspended_by_us) {
        zmk_rgb_underglow_get_state(&underglow_was_on);
        if (underglow_was_on) {
            zmk_rgb_underglow_off();
        }
        suspended_by_us = true;
    } else if (!any_active && suspended_by_us) {
        if (underglow_was_on) {
            zmk_rgb_underglow_on();
        }
        suspended_by_us = false;
    }
#endif
}
