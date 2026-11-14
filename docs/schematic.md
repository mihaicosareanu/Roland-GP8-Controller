# Schematic Description: MIDI Controller for Roland GP-8

This document describes the hardware (schematic) of the project. It is the hardware reference for firmware development. Firmware behavior (what each switch sends, what the LEDs show, etc.) is defined separately in `firmware_spec.md`.

**Single source of truth:** all MCU pin assignments are listed in [section 2](#2-pin-mapping) and nowhere else, except in [section 3](#3-reserved--special-function-pins), which explains why certain pins are special. All other sections refer to signals by name (e.g. `LED_1`, `SW_9`, `POT`). If a pin changes, update section 2 (and section 3 if the pin is listed there).

Items marked **TBD** are intentionally unresolved at this stage and will be clarified in a later phase. Do not invent values for them. See the [TBD section](#9-tbd-items-to-be-clarified-later).

---

## 1. Overview

- **Microcontroller:** STM32F401RCT6 (LQFP64, Cortex-M4, 3.3 V logic)
- **Board:** STM32F401/411 development board (exact model TBD). The board provides the power regulation and a USB port.
- **Peripherals:**
  - 1x 7-segment LED display (common anode, with decimal point), static drive
  - 11x LEDs, each driven through an N-channel MOSFET
  - 11x switches, active LOW, with external pull-ups
  - 1x potentiometer (ADC input)
  - 1x MIDI interface (DIN-5 IN and OUT) on USART1

---

## 2. Pin Mapping

This is the only complete pin list in this document.

| Signal | MCU Pin | Function | Direction | Active level |
|---|---|---|---|---|
| SEG_A | PA0 | GPIO | Output | HIGH = ON |
| SEG_B | PA1 | GPIO | Output | HIGH = ON |
| SEG_C | PA2 | GPIO | Output | HIGH = ON |
| SEG_D | PA3 | GPIO | Output | HIGH = ON |
| SEG_E | PA4 | GPIO | Output | HIGH = ON |
| SEG_F | PA5 | GPIO | Output | HIGH = ON |
| SEG_G | PA6 | GPIO | Output | HIGH = ON |
| SEG_DP | PA7 | GPIO | Output | HIGH = ON |
| LED_1 | PA8 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_2 | PA9 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_3 | PA10 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_4 | PA11 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_5 | PA12 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_6 | PA13 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_7 | PA14 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_8 | PA15 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_9 | PB15 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_10 | PC13 | GPIO (MOSFET gate) | Output | HIGH = ON |
| LED_11 | PC14 | GPIO (MOSFET gate) | Output | HIGH = ON |
| SW_1 | PB1 | GPIO | Input | LOW = pressed |
| SW_2 | PB2 | GPIO | Input | LOW = pressed |
| SW_3 | PB3 | GPIO | Input | LOW = pressed |
| SW_4 | PB4 | GPIO | Input | LOW = pressed |
| SW_5 | PB5 | GPIO | Input | LOW = pressed |
| SW_6 | PB8 | GPIO | Input | LOW = pressed |
| SW_7 | PB9 | GPIO | Input | LOW = pressed |
| SW_8 | PB10 | GPIO | Input | LOW = pressed |
| SW_9 (Group) | PB12 | GPIO | Input | LOW = pressed |
| SW_10 (Bank) | PB13 | GPIO | Input | LOW = pressed |
| SW_11 (Pedal_Mode) | PB14 | GPIO | Input | LOW = pressed |
| POT | PB0 | ADC1_IN8 | Analog input | n/a |
| MIDI_TX | PB6 | USART1_TX (AF7) | Output | n/a |
| MIDI_RX | PB7 | USART1_RX (AF7) | Input | n/a |

---

## 3. Reserved / Special-Function Pins

Several pins used in this design have a special function at reset or on the board. The design decisions below are deliberate. This is the one place besides section 2 where pin names appear, because the pin itself is the subject.

| Pin(s) | Default function | Decision for this project |
|---|---|---|
| PA13, PA14 | SWDIO, SWCLK | **SWD is not used.** Firmware configures them as plain GPIO outputs (`LED_6`, `LED_7`). |
| PA15, PB3, PB4 | JTDI, JTDO/SWO, NJTRST | **JTAG is not used.** Firmware configures them as plain GPIO (`LED_8`, `SW_3`, `SW_4`). |
| PA11, PA12 | USB OTG FS D-, D+ | Used as GPIO outputs (`LED_4`, `LED_5`). The USB peripheral must **not** be enabled in firmware. See TBD. |
| PC13 | RTC/tamper pin; low-current, low-speed output | Used as GPIO output driving a MOSFET gate (`LED_10`, negligible current). |
| PC14 | OSC32_IN (LSE crystal pin) | Used as GPIO output (`LED_11`). LSE/RTC must **not** be enabled in firmware. |

Notes for firmware:
- Because the SWD pins are repurposed, once firmware reconfigures them as GPIO, SWD debugging is no longer available. Programming is done through the board's USB DFU bootloader (or the UART bootloader), selected with BOOT0.
- The USB port on the board is used for power and programming only, not as a USB-MIDI device.

---

## 4. 7-Segment Display

- Type: 7-segment LED, common anode, with decimal point.
- Drive: **static drive** (no multiplexing). Each segment (`SEG_A` to `SEG_G`, `SEG_DP`) has its own dedicated MCU pin.
- Logic as seen by firmware: **pin HIGH = segment ON, pin LOW = segment OFF.**

Current-limiting resistor values and the exact driving circuit that realizes the HIGH = ON logic: **TBD**.

---

## 5. LEDs (MOSFET-driven)

There are 11 discrete LEDs (`LED_1` to `LED_11`). Each one uses the same circuit:

```
+3.3V --- [R] --- LED anode
                  LED cathode --- MOSFET drain
                                  MOSFET source --- GND
                                  MOSFET gate  <--- MCU pin (LED_n signal)
```

- Logic as seen by firmware: **pin HIGH = LED ON, pin LOW = LED OFF.**
- Firmware configures these pins as push-pull outputs.

MOSFET part number, resistor values, LED colors, and gate pull-down: **TBD**.

---

## 6. Switches

There are 11 switches (`SW_1` to `SW_11`). Each switch connects its MCU pin to GND when pressed.

- **Active LOW** (pressed = LOW, released = HIGH).
- Each switch pin has an **external 10 kΩ pull-up resistor to +3.3V**.
- Firmware must configure these pins as **inputs with no internal pull-up/pull-down**.

Special roles (all others are defined in `firmware_spec.md`):

| Switch | Role |
|---|---|
| SW_9 | Group switch |
| SW_10 | Bank switch |
| SW_11 | Pedal_Mode switch |

Debounce method (hardware vs. software) and switch type (momentary vs. latching): **TBD**. Until defined, firmware should assume momentary switches with software debouncing.

---

## 7. Potentiometer (ADC)

```
+3.3V ---+
         |
        [POT]---- wiper ----+---- POT signal (ADC input)
         |                  |
GND  ----+                [100 nF]
                            |
                           GND
```

- One outer pin to GND, the other outer pin to +3.3V, wiper to the `POT` signal (pin and ADC channel: see section 2).
- The pin is configured as analog input.
- A **100 nF capacitor** from the wiper to GND acts as a noise filter.
- Recommended potentiometer: 10 kΩ, linear taper.
- ADC reference is the 3.3V supply (VDDA/VREF+), so full scale is 3.3V. Firmware may use up to 12-bit resolution. Sampling time and filtering are firmware choices.

---

## 8. MIDI Interface (DIN-5)

### 8.1 UART configuration

Pins: `MIDI_TX` and `MIDI_RX` (see section 2).

| Parameter | Value |
|---|---|
| Peripheral | **USART1** (alternate function AF7) |
| Baud rate | **31250** |
| Frame format | 8 data bits, no parity, 1 stop bit (8N1) |
| Logic level at MCU | 3.3V |

- **MIDI OUT:** data leaves the MCU on `MIDI_TX`.
- **MIDI IN:** data enters the MCU on `MIDI_RX`.

### 8.2 Electrical interface

The MCU pins **must not** be connected directly to the DIN connectors. MIDI is a 5 mA current loop and requires the following circuit stages (reference design, final component values TBD):

**MIDI IN** (DIN-5 pins 4 and 5 to `MIDI_RX`):

```
DIN pin 4 --- [R ~220 ohm] ---+--- optocoupler LED anode
DIN pin 5 -------------------+--- optocoupler LED cathode
                    (protection diode, e.g. 1N4148, across the optocoupler LED, reverse polarity)

Optocoupler output (e.g. 6N138) ---> MIDI_RX, with pull-up to 3.3V
```

- Galvanic isolation through an optocoupler (e.g. 6N138 or equivalent) is required for MIDI IN.
- The signal at `MIDI_RX` must idle HIGH (UART idle level) with logic levels compatible with 3.3V.

**MIDI OUT** (`MIDI_TX` to DIN-5 pins 4 and 5):

```
MIDI_TX ---> buffer (non-inverting) ---> [R] ---> DIN pin 5
Supply (+3.3V or +5V) -----------------> [R] ---> DIN pin 4
DIN pin 2 --- shield / ground
```

- A buffer or transistor stage is required so that the MCU pin does not source or sink the loop current directly.
- A MIDI current loop conducts when the line is at logic 0 (UART start bit), so the TX signal drives the loop non-inverted.
- Series resistors follow the MIDI specification for the chosen supply voltage (3.3V or 5V), TBD.

---

## 9. TBD Items (To Be Clarified Later)

These items do not block firmware development but must be resolved before the hardware is finalized. Coding agents must not assume values for them.

1. **Exact board model** (STM32F401/411 board) and which pins are physically broken out.
2. **Power:** source of the +3.3V rail and whether a +5V rail is needed (for MIDI circuits).
3. **7-segment display:**
   - Current-limiting resistor values.
   - The driver circuit that makes the logic *pin HIGH = segment ON* (a common anode display lights when its segment pin is pulled LOW, so this requires an inverting driver per segment, or a different display type). The firmware contract in section 4 (HIGH = ON) stays as written unless the hardware changes.
4. **LED circuit:** resistor values, LED colors/currents, MOSFET part number (logic-level N-channel, Vgs(th) well below 3.3V), and whether a gate pull-down resistor is fitted to keep LEDs off during reset.
5. **Switches:** momentary vs. latching, and hardware vs. software debounce.
6. **MIDI circuit:** final component values, optocoupler part number, buffer part, and loop supply voltage (3.3V or 5V). MIDI IN is **required**: the firmware (`PEDAL_MODE`) receives a response from the unit, so the optocoupler stage is part of the design and only its component values are TBD.
7. **USB and the USB-shared pins:** `LED_4` and `LED_5` use pins wired to the board's USB connector (see section 3). Verify that these LED circuits do not interfere with USB DFU programming, and decide whether they should move to other pins if they do.
8. **Onboard components on `LED_10` / `LED_11` pins:** verify whether the board has an onboard LED on the `LED_10` pin and a 32 kHz crystal on the `LED_11` pin (see section 3), and whether this interferes with the LED circuits.
9. **Switch and LED functions:** the role of `SW_1` to `SW_8` and the mapping between switches and LEDs. This belongs to `firmware_spec.md`.
