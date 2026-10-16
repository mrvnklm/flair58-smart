#pragma once
// Click counting for the real button S1, done in a GPIO interrupt with timestamps.
//
// Why not on_multi_click: ESPHome's GPIO binary sensor keeps only the latest pin level from its interrupt
// and evaluates it in the main loop. Every blocking kettle HTTP request (~0.1-0.2 s, every 2 s) stalls that
// loop, so a short press or a short gap inside such a window is lost or two presses merge – triple presses
// were often missed. Here the ISR records every press itself; the loop only reads the result.
//
// Our own MOSFET presses pull the same line: a press that starts while `ignore` is true is not counted.
#include <cstdint>
#include <string>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace button_clicks {

static const int64_t MIN_PRESS_US = 20000;    // shorter = contact bounce
static const int64_t MIN_GAP_US = 30000;      // bounce after a release
static const int64_t MAX_PRESS_US = 600000;   // longer = the Flair's own on/off press, not a click

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static gpio_num_t pin;
static volatile const bool *ignore = nullptr;
static volatile bool pressed = false, ours = false, invalid = false;
static volatile int64_t press_start_us = 0, release_us = 0;
static volatile int count = 0;
static const int LOG_N = 6;                    // press lengths and gaps of the current sequence, for tuning
static volatile int press_ms[LOG_N], gap_ms[LOG_N];

static void IRAM_ATTR isr(void *) {
  const int64_t now = esp_timer_get_time();
  const bool low = gpio_get_level(pin) == 0;  // S1 is active-low
  portENTER_CRITICAL_ISR(&mux);
  if (low && !pressed && now - release_us >= MIN_GAP_US) {
    pressed = true;
    if (count > 0 && count <= LOG_N) gap_ms[count - 1] = (int) ((now - release_us) / 1000);
    press_start_us = now;
    ours = ignore != nullptr && *ignore;
  } else if (!low && pressed && now - press_start_us >= MIN_PRESS_US) {
    pressed = false;
    release_us = now;
    if (!ours) {
      if (now - press_start_us > MAX_PRESS_US) invalid = true;
      else {
        if (count < LOG_N) press_ms[count] = (int) ((now - press_start_us) / 1000);
        count++;
      }
    }
  }
  portEXIT_CRITICAL_ISR(&mux);
}

// Call once after setup. The pin must not have ESPHome's own interrupt (use_interrupt: false).
inline void setup(int gpio, const bool *ignore_flag) {
  pin = (gpio_num_t) gpio;
  ignore = ignore_flag;
  gpio_install_isr_service(0);  // ESP_ERR_INVALID_STATE if already installed – fine
  gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add(pin, isr, nullptr);
  gpio_intr_enable(pin);
}

// Returns the number of clicks once a sequence is complete (released and `quiet_ms` quiet), 0 otherwise.
// A sequence that contained a long press returns 0. `timing` gets e.g. "press 150 / gap 120 / press 160".
inline int take(uint32_t quiet_ms, std::string &timing) {
  const int64_t now = esp_timer_get_time();
  int n = 0;
  int p[LOG_N], g[LOG_N];
  portENTER_CRITICAL(&mux);
  // A spike shorter than MIN_PRESS_US leaves `pressed` set with the line already high again – clear it.
  if (pressed && gpio_get_level(pin) != 0 && now - press_start_us > 100000) {
    pressed = false;
    release_us = now;
  }
  if (!pressed && (count > 0 || invalid) && now - release_us >= (int64_t) quiet_ms * 1000) {
    n = invalid ? 0 : count;
    for (int i = 0; i < LOG_N; i++) { p[i] = press_ms[i]; g[i] = gap_ms[i]; }
    count = 0;
    invalid = false;
  }
  portEXIT_CRITICAL(&mux);
  timing.clear();
  for (int i = 0; i < n && i < LOG_N; i++) {
    if (i > 0) timing += " / gap " + std::to_string(g[i - 1]) + " / ";
    timing += "press " + std::to_string(p[i]);
  }
  return n;
}

}  // namespace button_clicks
