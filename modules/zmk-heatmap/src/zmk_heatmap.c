/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>

LOG_MODULE_REGISTER(zmk_heatmap, CONFIG_ZMK_HEATMAP_LOG_LEVEL);

static int zmk_heatmap_listener_cb(const zmk_event_t *eh) {
    struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev != NULL) {
        if (ev->state) {
            uint8_t layer = zmk_keymap_highest_layer_active();
            int64_t timestamp = k_uptime_get();
            LOG_INF("HM:%u,%u,%lld", ev->position, layer, timestamp);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zmk_heatmap, zmk_heatmap_listener_cb);
ZMK_SUBSCRIPTION(zmk_heatmap, zmk_position_state_changed);
