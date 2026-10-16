# The Flair 58 control board

Measured on board **HL-01-V2.2, dated 2024-08-14**. Other revisions may differ; the board marking is on the front.
Entries marked *probable* come from photos and part markings, not from measurements.

Photos: [front](../photos/board-front.jpg) · [back](../photos/board-back.jpg)

## Overview

- The machine runs from an external 24 V power supply. There is no mains voltage inside.
- One small PCB in the lower housing. Front: push button S1 and three green LEDs. Back: everything else.
- A 6-wire harness connects it to the brew head.

| Pad (front) | Wire | Function |
|---|---|---|
| GND (黑) | black | ground from the 24 V supply |
| 24V (红) | red | +24 V (measured 24.6 V) |
| NTC | green, yellow | NTC in the brew head heater pad |
| HOT | white, brown | heater element (back side labelled P+ / P-) |

## Components (back)

| Ref | Part | Role |
|---|---|---|
| U2 | SOP-14 MCU, marking read as an FMD logo + lot code, no part number | main controller. *Probable:* a Fremont Micro Devices 8-bit MCU, FT61F022A class |
| SOT-89 next to CON2 | `7550P` | 5 V linear regulator fed straight from 24 V (measured 24 V in, 5.005 V out). 7550 type, 100 mA class |
| Q6 | D-PAK, `50P06L` | *probable:* heater switch. A P-channel part, so likely high side (P+ switched to 24 V) |
| Q5 | SOT-23 | *probable:* gate driver for Q6 |
| Buzzer | round SMD | ready beep |
| R6, R11, C4 | next to the NTC pads | *probable:* NTC divider and filter |
| CON2 | unpopulated 5-pin header `5V PA1 PA6 PA0 GND` | *probable:* factory programming header (see below) |

From the [Frankenflair 58](https://something.fromnothing.blog/posts/frankenflair/) build, not re-measured here:
NTC 100 kΩ at room temperature, sitting in the silicone heater pad; heater ≈ 7 Ω cold, ≈ 82 W at 24 V; stock PSU 24 V / 5 A.

## Signals

| Signal | Where | Level |
|---|---|---|
| Button S1 | S1 leg wired to **U2 pin 4** (0 Ω, direct trace) | **active-low**: idles at 5 V through the Flair's pull-up, pressed = 0 V |
| LEDs D2/D3/D4 | anode legs | cathode on GND (measured on one LED, assumed for all three), U2 sources the anode through a resistor. Lit: just under **2.2 V**; off: expected ~0 V |
| 24 V | 24V / GND pads | 24.6 V |

### Button timing (via the ESP)

| Press | Effect |
|---|---|
| 1300 ms | on / off (the manual says hold 3 s; 1300 ms was enough) |
| 50 ms | next preset: LOW → MEDIUM → HIGH (wrap-around to LOW assumed) |
| short, while off | ignored by the Flair |

Minimum lengths were not searched further.

### LED logic (Flair manual, matches what the LEDs showed)

| State | LEDs |
|---|---|
| Off | all off |
| Heating | the selected preset's LED blinks (~1 s period), lower ones solid |
| Ready | solid bar: LOW = left · MEDIUM = left + middle · HIGH = all three |
| Power-on | starts on LOW (~85 °C). MEDIUM ~90 °C, HIGH ~95 °C |

The LED lights while the power-on press is still held. A short press sent at that moment merged into the long press
and was lost.

2.2 V is below the ESP32's guaranteed digital high (~2.5 V), so the firmware reads the LEDs on ADC1 pins with a
1.2 V on / 0.6 V off hysteresis. ADC2 does not work while Wi-Fi is on.

## NTC observations

U2 powers the NTC only in short pulses, and the node is extremely high-impedance:

| Test | Observation |
|---|---|
| Multimeter on the NTC pads, machine warm | ~0 V with occasional flicker to ~0.1 V |
| ESP pin on the NTC pad through 1 kΩ, later 3 × 1 kΩ | the Flair's LEDs flicker every 0.1–0.5 s: its own temperature measurement is disturbed |
| Bare wire soldered to the pad, other end free | touching the wire makes an LED glow faintly |

A passive tap therefore did not work in this build. Ideas for an own head sensor: [ideas.md](ideas.md#head-temperature).

## Dead ends

| Idea | Result |
|---|---|
| CON2 as UART or control header | PA0/PA1/PA6 read 0 V in standby and running. None is connected to S1 (16–32 MΩ). Pulling each high through 1 kΩ did nothing. *Probable* match with the ISP pins of an FT61F022A (PA1 = ISPDAT, PA0 = ISPCLK, not checked by continuity). Reflashing U2 would need FMD's programmer and a full replacement firmware |
| Powering the ESP from CON2 `5V` | that is the 7550 LDO (~100 mA), fed straight from 24 V |
| Continuity beeper on the MCU pins | the meter current back-powers U2 through its clamp diodes; the Flair's buzzer chirps on every pin. The Ω range gives usable readings |
| Driving S1 directly from a GPIO | first attempt drove the line inverted (idle low = button held) and the machine reacted to the edges. Open-drain works, but the GPIO then sees the Flair's 5 V pull-up, and an unpowered ESP can pull the line low through its clamp diodes, which the Flair may take as a press (the direct-drive firmware had a cold-boot guard for it). Hence the MOSFET |
| ESP and Flair on separate supplies | with one side powered and the other not, the signal wires back-fed and disturbed the Flair |
