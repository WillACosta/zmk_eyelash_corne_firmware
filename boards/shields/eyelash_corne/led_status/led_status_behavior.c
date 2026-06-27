/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * LED Status Behavior
 *
 * A simple ZMK behavior that triggers the LED status queries when bound to
 * a key or combo.  The behavior accepts a parameter selecting which status
 * to display:
 *
 *   LED_QUERY_HOST    (0) – host connection status (FR-3)
 *   LED_QUERY_SPLIT   (1) – split connection status (FR-4)
 *   LED_QUERY_BATTERY (2) – battery level (FR-5)
 *
 * Keymap usage:
 *
 *   #include "led_status/led_status_behavior.h"
 *
 *   / {
 *       behaviors {
 *           led_query: led_query {
 *               compatible = "zmk,behavior-led-status";
 *               #binding-cells = <1>;
 *           };
 *       };
 *
 *       combos {
 *           compatible = "zmk,combos";
 *           led_battery { bindings = <&led_query LED_QUERY_BATTERY>; ... };
 *           led_host    { bindings = <&led_query LED_QUERY_HOST>;    ... };
 *           led_split   { bindings = <&led_query LED_QUERY_SPLIT>;   ... };
 *       };
 *   };
 */

#define DT_DRV_COMPAT zmk_behavior_led_status

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <drivers/behavior.h>

#include "led_status.h"
#include "led_status_behavior.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Forward-declaration of the query helpers from led_status.c */
extern void led_status_query_host(void);
extern void led_status_query_split(void);
extern void led_status_query_battery(void);

static int led_status_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                              struct zmk_behavior_binding_event event)
{
    switch (binding->param1) {
    case LED_QUERY_HOST:
        led_status_query_host();
        break;
    case LED_QUERY_SPLIT:
        led_status_query_split();
        break;
    case LED_QUERY_BATTERY:
        led_status_query_battery();
        break;
    default:
        LOG_WRN("Unknown LED query type: %d", binding->param1);
        break;
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int led_status_keymap_binding_released(struct zmk_behavior_binding *binding,
                                               struct zmk_behavior_binding_event event)
{
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api led_status_behavior_driver_api = {
    .binding_pressed  = led_status_keymap_binding_pressed,
    .binding_released = led_status_keymap_binding_released,
};

static int led_status_behavior_init(const struct device *dev)
{
    return 0;
}

BEHAVIOR_DT_INST_DEFINE(0, led_status_behavior_init, NULL, NULL, NULL,
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &led_status_behavior_driver_api);
