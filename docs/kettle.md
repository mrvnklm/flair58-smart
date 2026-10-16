# Fellow Stagg EKG Pro

The **EKG Pro** has Wi-Fi and BLE (an ESP32 inside). The older **EKG+** is BLE-only with a different protocol and is
not covered here.

## Firmware decides everything

| Firmware | Local HTTP CLI over Wi-Fi | On/off | Verdict |
|---|---|---|---|
| 1.1.75SSP / 1.1.76SSP | full: state and commands | Wi-Fi | best for local control; what `kettle_wifi.yaml` needs |
| 1.2.24 | muted | none reliable | avoid |
| 1.2.26 | muted, native HTTP partly | **BLE only**; Wi-Fi pauses while heating | needs a BLE path (not built, see [ideas.md](ideas.md)) |

Fellow's changelog: <https://help.fellowproducts.com/hc/en-us/articles/9593179929115>

- With internet access the kettle downloads updates. Opening the Fellow app triggers an update too (over BLE).
- Going back from 1.2.x needs a 1.1.75SSP image; the HA integration
  [bramboe/stagg-ekg-plus-ha](https://github.com/bramboe/stagg-ekg-plus-ha) has firmware tools for that in its beta
  channel. Untested here.

### How the tested kettle stays on 1.1.75SSP

- It sits on an SSID without internet access, reachable from Home Assistant and the ESP.
- It has a fixed DHCP lease; the ESP addresses it by IP (no mDNS name).
- The HA integration `fellow_stagg` ([bramboe/stagg-ekg-plus-ha](https://github.com/bramboe/stagg-ekg-plus-ha),
  installed via [HACS](https://github.com/hacs/integration)) shows the firmware version and offers
  `Block auto-updates` and, on legacy firmware, `Lock firmware`.

## HTTP CLI (1.1.x)

`http://<ip>/cli?cmd=<command>`, unauthenticated. `tools/kettle_cli.sh <ip> <command>` wraps it.

| Command | Effect |
|---|---|
| `help`, `state`, `fwinfo`, `prtsettings` | read-only |
| `setstate S_Heat` / `setstate S_Off` | start / stop heating |
| `setsetting settempr <°F>` | target temperature; the firmware stores it in °F, the ESP converts |
| `buz <freq_hz> <duty_13bit> <dur_ms>` | one tone, also while off |
| `buz` with exactly 9 values | 3 tones back to back with the kettle's own timing. 12 values are rejected |

**Encoding quirk:** spaces must be sent as `+`, not `%20`. The kettle does not URL-decode
(`cmd=refresh%202` arrives as `refresh%202`, `cmd=refresh+2` as `refresh 2`).

Fields in `state` used by the ESP: `mode=`, `tempr=` (`nan` while lifted), `ketl=` (contains `nw 1` = no water).

### Modes

| Mode | Meaning |
|---|---|
| `S_OFF` | off, also while lifted when it was off |
| `S_STARTUPTOTEMPR` → `S_HEAT` → `S_HOLD` | switched on → heating → holding at target |
| `S_STANDBY` | lifted while on. Back on the base it resumes `S_HEAT` by itself |
| `…+TSET` | the dial is being turned. Cut the suffix before classifying |

## BLE (findings on 1.1.75SSP)

Tested through an [ESPHome Bluetooth proxy](https://esphome.io/components/bluetooth_proxy/) with `tools/kettle_proxy.py`
([aioesphomeapi](https://github.com/esphome/aioesphomeapi) GATT, no reflash per test).

| Item | Result |
|---|---|
| Address / name | on the tested kettle: BLE MAC = Wi-Fi MAC + 2. Name `EKG-xx-xx-xx` in the scan response only |
| Connections | **one central at a time.** A stale connection blocks every other central (HCI 0x3e) until the kettle is power-cycled. HTTP `state` shows `ble conn=<n>` |
| GATT | same layout as 1.2.26: provisioning service `021a9004…` + kettle service `7aebf330…` with characteristics `2291c4b1`…`b9` |
| B1 status | 16 zero bytes, no notifications ⇒ **no status over BLE on 1.1.75** |
| B5 settings | readable; target stored as °F (`0x80c9` = 201 °F = 94 °C), not the 1.2.26 encoding |
| B6 console | accepts CLI text writes without a reply. `setstate S_Heat` / `S_Off` switch the kettle within ~2 s |
| Proxy pitfall | the ESPHome proxy drops every GATT connection as soon as its advertisement subscriber unsubscribes. Keep the subscription open |

So on 1.1.x, BLE can send commands but not report state; Wi-Fi is the better path. On 1.2.26 BLE is the only
path, with status on B1 and commands on B6 according to the integration sources (not verified here).

## Security

The CLI has no authentication: anyone on that network can switch the kettle on. The tested kettle sits on an
isolated segment that only Home Assistant and the ESP can reach.

## Links

- <https://github.com/bramboe/stagg-ekg-plus-ha> (HA integration, Wi-Fi CLI and BLE)
- <https://github.com/tomtastic/stagg-ekg-pro> (protocol notes, archived)
