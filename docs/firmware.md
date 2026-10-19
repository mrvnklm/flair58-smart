# Firmware

[ESPHome](https://esphome.io) (tested with 2026.9, esp-idf framework). One main config plus two optional packages.
Wiring, flashing and the switch defaults are in the [README](../README.md#firmware).

| File | Content |
|---|---|
| `esphome/flair58.yaml` | pins, button presses, LED decoding, `Powered` / `Heating` / `Ready` / `Status`, `Preset` select, safety off, Start on HIGH, click handling |
| `esphome/button_clicks.h` | counts clicks of the real button in a GPIO ISR |
| `esphome/packages/kettle_wifi.yaml` | kettle poll (2 s), kettle sensors, coupling, melodies |
| `esphome/packages/kettle_melodies.h` | melody table and the chunker for the kettle buzzer |
| `esphome/packages/auto_off.yaml` | auto off after the shot |

Without the kettle or auto off, delete the line under `packages:` (or the whole block). The main config compiles
on its own; the packages talk to it only through a few globals and scripts listed in their headers.

## How the state is derived

| Entity | Rule |
|---|---|
| `LED n` | ADC every 100 ms, hysteresis 1.2 V on / 0.6 V off |
| `Powered` | any LED lit or blinking (blink gaps bridged for 1.2 s); left and middle LED going dark together on MEDIUM/HIGH = off after 250 ms (the left LED only blinks on LOW) |
| `Heating` | an LED changed state twice within 1.2 s, and the last change is recent |
| `Ready` | powered and not heating for 3 s |
| `Preset index` / `Preset` | highest active LED: 1 = LOW, 2 = MEDIUM, 3 = HIGH |
| `Status` | `off` / `heating` / `ready` / `settling` |

## Pressing safely

S1 only **toggles**, and the ESP only knows the power state from the LEDs. A wrong guess would turn an "off" into
an "on". So:

- Every press goes through one queued script, so a short press never cuts into a long one.
- Every power change goes through `toggle_verified`: never two long presses within 3 s (a request in that window
  waits), re-check the state right before pressing, then wait up to 3 s for the LEDs to agree. On failure it logs a
  warning and fires `esphome.flair_toggle_unconfirmed`.
- A click on the real button stops a running toggle and starts its own, so it is never dropped. A click within
  2.5 s of a start click is ignored (the LEDs light ~1.3 s after it).
- Automatic offs (safety, kettle, auto off, click, `Power` switch) retry up to 3 times.
- The `Power` switch never acts at boot (`restore_mode: DISABLED`).

## Behaviour

| Trigger | Action | Switch (default) |
|---|---|---|
| Powered for 45 min | Flair off, independent of all switches and of HA | – |
| Flair switched on (after ≥ 2 s off, not in the first 15 s after boot) | wait for the release, 100 ms, then short presses one at a time until the right LED lights; each press must light the next LED within 1 s, at most 3 presses. Needed a retry or missed HIGH: event `start_high_retry` | `Start on HIGH` (off) |
| One short press on the real button while off | Flair on | `Short press starts` (on) |
| One short press on the real button while on | Flair off (the Flair steps one preset first, then the ESP's long press switches it off) | `Short press stops` (off) |
| **Kettle package** | | |
| Kettle starts heating, Flair off | Flair on | `Kettle sync` (on) |
| Flair switched on, kettle on its base, has water, not heating | kettle target set, kettle heats (after Start on HIGH) | `Kettle sync` |
| Kettle stops heating and is still on its base after 2 s | Flair off; not if the stop came from our own `S_Off` | `Kettle sync` |
| The ESP switches the Flair off (any of its own off presses) | kettle off right after the press | `Kettle sync` |
| Kettle back on its base after a lift (`S_STANDBY` → `S_HEAT`) | nothing: a resume, not a start | – |
| Flair goes off (any cause), kettle not off, Flair still off after 2 s and one fresh poll | kettle off | `Kettle sync` |
| Kettle heats after our own `S_Heat` | nothing: not a kettle start | – |
| ≥ 2 short presses on the real button while off | random melody on the kettle buzzer | `Melody on double press` (on) |
| Auto off fired (needs both packages) | kettle off together with the Flair, then a 1 s jingle | `Auto off jingle` (on) |
| **Auto off package** | | |
| First reheat after Ready, not within 20 s of any press | `Auto off after` s later (25): Flair off | `Auto off after shot` (off) |
| Hot kettle (≥ 85 °C) lifted ≤ 180 s before Ready, then no further lift and no reheat (needs the kettle package) | Ready held `Auto off fallback after` s (150) without a break: Flair off. Every new Ready restarts the count, any lift disarms it | `Auto off fallback` (off) |
| First hot kettle lift (≥ 85 °C) while on, no shot seen (water poured while heating, Ready never came; needs the kettle package) | `Auto off pour cap` s (300) after that lift: Flair off, unless the reheat path is already counting. Later lifts do not restart it | `Auto off fallback` (off) |

Typical timings (reference build): kettle on → Flair on 1.5–3.5 s · kettle off → Flair off 4–10 s ·
short press → HIGH 1.65 s after release · ESP switches the Flair off → kettle off within ~1 s.

## Home Assistant interface

| Kind | Entities |
|---|---|
| Controls | `Power`, `Preset`, `Power toggle`, `Next preset`, `Restart`; kettle: `Play kettle melody` |
| State | `Powered`, `Heating`, `Ready`, `Status`, `Preset index`, `Button`, `LED 1–3`; kettle: `Kettle mode`, `Kettle heating`, `Kettle on base`, `Kettle temperature`, `Kettle reachable` |
| Switches | see the behaviour table |
| Config | `Long press`, `Short press`, `Press gap`; kettle: `Kettle IP`, `Kettle target`; auto off: `Auto off after`, `Auto off fallback after`, `Auto off pour cap` |
| Diagnostics | `LED n voltage`, `Button line (raw)`, `Wi-Fi signal`, `Blink gap max` (longest dark phase of a blinking LED since power-on; to size `blink_window_ms`, reference build: 800 ms) |
| Events | `esphome.flair_button` (`type`: `short_press` / `long_press`, `machine`), `esphome.espresso_sync` (`action`), `esphome.flair_toggle_unconfirmed` (`wanted`), `esphome.flair_clicks` (`clicks`, `timing`, `machine`) |

With `friendly_name: Flair 58`, entity IDs come out as `switch.flair_58_power`, `binary_sensor.flair_58_ready`,
`sensor.flair_58_kettle_mode` and so on.

## Why some things are built the way they are

| Thing | Reason |
|---|---|
| Clicks counted in an own ISR (`button_clicks.h`) | ESPHome's GPIO sensor keeps only the latest level and evaluates it in the main loop. Every kettle HTTP request blocks that loop for 150–470 ms, so presses and gaps inside such a window got lost. Measured fast taps: presses 29–62 ms, gaps 48–73 ms |
| HTTP never runs while a press is held | a blocked loop would stretch a 50 ms preset press into a 1300 ms on/off press |
| `power_save_mode: none` | a kettle request from the ESP took 100–640 ms with light sleep, 150–470 ms without |
| Kettle poll backs off to 10 s when unreachable | each failed request blocks the loop up to the 1 s timeout |
| Melodies in chunks of 3 tones | one request per note is too slow; the kettle's `buz` takes 9 values = 3 tones played back to back |
