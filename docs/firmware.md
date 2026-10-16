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
| `Powered` | any LED lit or blinking (blink gaps bridged for 1.2 s) |
| `Heating` | an LED changed state twice within 1.2 s, and the last change is recent |
| `Ready` | powered and not heating for 3 s |
| `Preset index` / `Preset` | highest active LED: 1 = LOW, 2 = MEDIUM, 3 = HIGH |
| `Status` | `off` / `heating` / `ready` / `settling` |

## Pressing safely

S1 only **toggles**, and the ESP only knows the power state from the LEDs. A wrong guess would turn an "off" into
an "on". So:

- Every press goes through one queued script, so a short press never cuts into a long one.
- Every power change goes through `toggle_verified`: re-check the state right before pressing, never toggle within
  3 s of a long press, verify via the LEDs 4 s later. On failure it logs a warning and fires
  `esphome.flair_toggle_unconfirmed`.
- Automatic offs (safety, kettle, auto off) retry up to 3 times.
- The `Power` switch never acts at boot (`restore_mode: DISABLED`).

## Behaviour

| Trigger | Action | Switch (default) |
|---|---|---|
| Powered for 45 min | Flair off, independent of all switches and of HA | – |
| Flair switched on (after ≥ 2 s off, not in the first 15 s after boot) | wait for the release, 100 ms, two short presses → HIGH | `Start on HIGH` (off) |
| One short press on the real button while off | Flair on | `Short press starts` (on) |
| **Kettle package** | | |
| Kettle starts heating, Flair off | Flair on | `Kettle sync` (on) |
| Flair switched on, kettle on its base, has water, not heating | kettle target set, kettle heats | `Kettle sync` |
| Kettle stops heating and is still on its base after 2 s | Flair off | `Kettle sync` |
| Kettle back on its base after a lift (`S_STANDBY` → `S_HEAT`) | nothing: a resume, not a start | – |
| Flair goes off (any cause), kettle not off, Flair still off after 2 s | kettle off | `Kettle sync` |
| ≥ 3 short presses on the real button while off | random melody on the kettle buzzer | `Melody on triple press` (on) |
| Auto off fired (needs both packages) | 1 s jingle on the kettle; the kettle then goes off through the Flair-off row | `Auto off jingle` (on) |
| **Auto off package** | | |
| First reheat after Ready, not within 20 s of any press | `Auto off after` s later (25): Flair off | `Auto off after shot` (off) |

Typical timings (reference build): kettle on → Flair on 1.5–3.5 s · kettle off → Flair off 4–10 s ·
short press → HIGH 1.65 s after release.

## Home Assistant interface

| Kind | Entities |
|---|---|
| Controls | `Power`, `Preset`, `Power toggle`, `Next preset`, `Restart`; kettle: `Play kettle melody` |
| State | `Powered`, `Heating`, `Ready`, `Status`, `Preset index`, `Button`, `LED 1–3`; kettle: `Kettle mode`, `Kettle heating`, `Kettle on base`, `Kettle temperature`, `Kettle reachable` |
| Switches | see the behaviour table |
| Config | `Long press`, `Short press`, `Press gap`; kettle: `Kettle IP`, `Kettle target`; auto off: `Auto off after` |
| Diagnostics | `LED n voltage`, `Button line (raw)`, `Wi-Fi signal` |
| Events | `esphome.flair_button` (`type`: `short_press` / `long_press`, `machine`), `esphome.espresso_sync` (`action`), `esphome.flair_toggle_unconfirmed` (`wanted`) |

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
