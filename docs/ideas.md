# Open points and ideas

Reports from other machines and kettles are welcome as issues.

## Open points

| Point | How to close it |
|---|---|
| Other board revisions than HL-01-V2.2 | photos of the board and the S1/LED measurements from [hardware.md](hardware.md) |
| Auto-off jingle in a real shot; ESP-side "Flair off → kettle off"; triple-press melody in 3-tone chunks | implemented, not yet observed at the machine |
| 10 kΩ in the LED lines | the reference build reads the LEDs directly |
| Button-read divider (100 kΩ + 150–220 kΩ to GND) | the reference build uses 100 kΩ only; measure the idle level |
| HA-side coupling (`kettle_coupling_ha.yaml`) with a 1.2.x kettle over BLE | the "Flair off → kettle off" automation never ran; no lift/resume guard like the ESP has |
| Preset cycle after HIGH (wrap-around to LOW assumed) | press `Next preset` on HIGH and watch the LEDs |
| Real head temperature at each preset | thermometer in the cylinder after the ready beep |
| Supply from the Flair's 24 V via buck | build and report; the reference build still runs from USB |
| Discrete MOSFET (2N7002 / BSS138) instead of the D4184 module | build and report |
| Kettle firmware 1.1.76SSP | does `kettle_wifi.yaml` work unchanged? |

## Backlog

- Ready chime: the kettle plays a tone when Flair **and** kettle are ready
- Shot counter and heat-up time sensors
- Find the kettle IP automatically (DHCP name, network scan, or the Wi-Fi MAC readable over BLE)
- Morning preheat at a set time in the firmware (no HA needed)
- BLE kettle path for firmware 1.2.26: status on B1, commands on B6, through the ESP's own BLE
- Antenna outside the housing (reference build: −69…−73 dBm inside)

## Head temperature

Tapping the stock NTC disturbed the Flair in this build ([hardware.md](hardware.md#ntc-observations)). Options:

| Option | Notes |
|---|---|
| Thin K-type thermocouple under the silicone heater pad (Kapton) + MAX31856/MAX31855 | as in the [Frankenflair](https://something.fromnothing.blog/posts/frankenflair/) build. Needs SPI pins and space |
| Thermocouple through the valve-plunger cavity into the cylinder | measures the water, not the pad; also from Frankenflair |
| CMOS op-amp follower soldered right at the NTC pad | experimental; needs a scope to prove it does not disturb the Flair |

## Full heater control (phase 2)

The ESP could regulate the heater itself: cut U2's drive to Q5, drive Q5 from the ESP, PID on an own head sensor.
Things such a build would want: an independent thermal cut-off in hardware, a gate pull-down so that a crashed ESP
means heater off, and a watchdog. Not started here; the [Frankenflair 58](https://something.fromnothing.blog/posts/frankenflair/)
build is a complete version of this, including a hardware comparator that cuts the heater above 120 °C.
