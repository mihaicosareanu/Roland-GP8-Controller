# Firmware Specification: MIDI Controller for Roland GP-8
 
This document describes how the firmware must behave. Hardware, pin assignments and electrical details are defined in `schematic.md`. Signals are referred to here by their schematic names (e.g. `SW_1`, `LED_3`, `POT`).
 
The document is written in parts. Items that refer to something defined in a later part are marked as such.
 
---
 
## 1. Common Components Behaviour
 
General requirements for the system components, valid independently of the operating mode (unless a later section explicitly says otherwise).
 
### 1.1 Switches
 
#### 1.1.1 Debouncing
 
All switches (`SW_1` to `SW_11`) require software debouncing:
 
- A **press** is valid only after the switch reads **LOW** continuously for **30 ms**.
- A **release** is valid only after the switch reads **HIGH** continuously for **30 ms**.
- The rules below operate on debounced states only. Raw switch levels are never used directly.
#### 1.1.2 Multiple switches pressed at once
 
Only one switch is accepted at a time.
 
1. **First press wins.** The accepted switch is the first one whose press becomes valid (passes the 30 ms debounce). If two presses become valid in the same debounce evaluation, the switch with the lowest number wins.
2. **Lockout.** While a switch is accepted, presses of all other switches are ignored. The lockout ends only when **all 11 switches** have a valid release. Until then, no new press is accepted.
3. **Release of the accepted switch.** If the accepted switch is released while another switch is still held, this counts as a normal release of the accepted switch. The switch that is still held stays ignored, and the lockout continues until all switches are released.
#### 1.1.3 Exception: PROGRAM_MODE combination
 
During `PROGRAM_MODE` (defined in a later section), the following combination is allowed:
 
- `SW_10` is pressed first and accepted as usual.
- While `SW_10` is still held, **one** switch from `SW_1` to `SW_8` is accepted as the second switch of the combination.
- Further presses during the combination are ignored. The lockout ends when all switches are released.
- The reverse order (a switch from `SW_1` to `SW_8` first, then `SW_10`) is **not** a combination. `SW_10` is ignored under the general rule in 1.1.2.
#### 1.1.4 Switches held at power-up
 
- At power-up, any switch that is already held is ignored until it has a valid release.
- **Exception:** if `SW_1` and `SW_2` are both held at power-up and both read LOW continuously for 30 ms, the controller enters `PROGRAM_MODE` directly.
  - After entering `PROGRAM_MODE` this way, `SW_1` and `SW_2` are ignored until they are released. After that, the normal `PROGRAM_MODE` rules apply (including the combination in 1.1.3).
  - This exception applies **only at power-up**.
  - No other switch combination is treated specially at power-up. Any other switch held at power-up is simply ignored until released.
### 1.2 ADC (`POT`)
 
#### 1.2.1 Definitions
 
| Name | Definition |
|---|---|
| `ADC` | The latest raw 12-bit reading of `POT` (0 to 4095), right-aligned, with no scaling. |
| `ADC_old` | The raw 12-bit `ADC` value at the time of the last accepted change. |
| `ADC_STEP` | `1 << 5` (32). This is the size of one MIDI value step (4096 / 128). |
| `CC_VALUE` | `ADC_old >> 5`, giving a 7-bit MIDI value (0 to 127). |
 
#### 1.2.2 Behavior
 
1. **Initialization:** `ADC_old` is initialized from the **first valid ADC reading after power-up**, and is **re-initialized from the current `ADC` reading every time `NORMAL_MODE` is entered** (including when entering it from `PROGRAM_MODE`). No MIDI message is sent at these points.
2. **Change detection:** on every new reading, if `abs(ADC_old - ADC) >= ADC_STEP`:
   - `ADC_old` is set to `ADC`.
   - A `MIDI_Control_Change` message is sent with the value `CC_VALUE` (message format, channel and controller number are defined in a later section).
3. Otherwise `ADC_old` is unchanged and nothing is sent.
Note: because the threshold equals one MIDI step, every accepted change of `ADC_old` results in a `CC_VALUE` different from the previous one. No separate "value changed" check is needed.
 
#### 1.2.3 Scope
 
- The ADC behavior is **not active in `PROGRAM_MODE`**: the `POT` is not evaluated and no MIDI messages are sent.
- Behavior in `NORMAL_MODE` and `PEDAL_MODE` is defined in later sections.
---
 
## 2. Firmware Overview
 
The foot controller operates in one of three modes:
 
- `PROGRAM_MODE`
- `NORMAL_MODE`
- `PEDAL_MODE`
### 2.1 Patch organization
 
- There are two groups of patches: **Group 0** and **Group 1**.
- Each group has 8 banks (**Bank 1** to **Bank 8**).
- Each bank has 8 patches (**Patch 1** to **Patch 8**).
### 2.2 Variables
 
| Variable | Storage | Initial value | Notes |
|---|---|---|---|
| `MIDI_Channel` | RAM, **persisted in flash** | `0` (also used if flash is empty or invalid) | Value 0 to 15. The only variable persisted across power cycles. |
| `Group` | RAM | `0` | Reset at every power-up. |
| `Bank` | RAM | `1` | Reset at every power-up. |
| `ADC_old` | RAM | First valid ADC reading; re-initialized on entering `NORMAL_MODE` (see section 1.2) | |
| `b1` to `b8` | RAM | Defined in a later section (`NORMAL_MODE`) | |
| `SWT` | RAM | Defined in a later section (`NORMAL_MODE`) | |
 
Only `MIDI_Channel` is persisted. All other variables live in RAM and are re-initialized at power-up.
 
### 2.3 Group indication
 
`LED_10` and `LED_11` display the current group. Exactly one of them is lit at any moment, in every mode:
 
| `Group` | `LED_10` | `LED_11` |
|---|---|---|
| 0 | ON | OFF |
| 1 | OFF | ON |
 
---
 
## 3. Power ON
 
At power-up the controller enters `PROGRAM_MODE` if the entry condition in section 1.1.4 is met (`SW_1` and `SW_2` both held and debounced). Otherwise it enters `NORMAL_MODE`.
 
---
 
## 4. PROGRAM_MODE
 
Purpose: select the MIDI channel (0 to 15) used by the controller.
 
### 4.1 Entering PROGRAM_MODE
 
On entry:
 
- `LED_1` to `LED_8` blink once: all ON for 250 ms, then all OFF.
- The decimal point of the 7-segment display (`SEG_DP`) turns ON and stays ON during `PROGRAM_MODE`.
- Segments `SEG_A` to `SEG_G` are OFF.
- `LED_9` is OFF.
- `LED_10` / `LED_11` show the group (section 2.3).
All timed LED sequences are non-blocking (timer-driven), so debouncing and the main loop keep running. Switch presses are ignored until the entry blink has finished.
 
### 4.2 Waiting for a selection
 
- Only `SW_1` to `SW_8` have an effect.
- `SW_9`, `SW_11`, and `SW_10` pressed alone (with no switch from `SW_1` to `SW_8`) do nothing.
- `ADC` is not evaluated and no MIDI messages are sent (section 1.2.3).
- There is no way to leave `PROGRAM_MODE` except by selecting a channel, or by a power cycle. This is intentional.
### 4.3 Channel selection
 
The channel range is decided when a switch from `SW_1` to `SW_8` is accepted, using the switch combination rule in section 1.1.3:
 
| Situation when `SW_n` (n = 1 to 8) is accepted | Channel selected |
|---|---|
| `SW_10` is **not** held | `n - 1` (channel 0 to 7) |
| `SW_10` was pressed first and is **still held** | `n + 7` (channel 8 to 15) |
 
Examples: `SW_1` gives channel 0, `SW_8` gives channel 7, `SW_10` + `SW_1` gives channel 8, `SW_10` + `SW_8` gives channel 15.
 
The selection is **confirmed when `SW_n` has a valid release** (consistent with the release-based handling used for the other switches). The channel range (0 to 7 or 8 to 15) is latched when `SW_n` is accepted and does not change if `SW_10` is released before `SW_n`.
 
On confirmation:
 
1. `MIDI_Channel` is updated in RAM.
2. `MIDI_Channel` is written to flash (section 4.5).
3. The exit sequence (section 4.4) starts.
### 4.4 Exit sequence
 
1. `SEG_DP` turns OFF.
2. `LED_1` to `LED_8` blink three times: 100 ms ON, 100 ms OFF each (600 ms total).
3. When the last blink ends, the controller enters `NORMAL_MODE`.
Switch presses are ignored during the exit sequence. The switch lockout (section 1.1.2) carries over into `NORMAL_MODE`: if switches are still held when `NORMAL_MODE` starts, no new press is accepted until all switches have been released.
 
### 4.5 Persistent storage of `MIDI_Channel`
 
- The STM32F401 has no EEPROM. `MIDI_Channel` is stored in a dedicated flash sector.
- It is written **only** when a channel selection is confirmed (section 4.3), before the exit blinks start.
- At power-up the value is read from flash. If flash is empty or the stored value is invalid (not 0 to 15), `MIDI_Channel` is `0`.
- Flash erase/write may block the CPU for a short time. This is acceptable at this point.

## 5. NORMAL_MODE
In NORMAL_MODE, we have this initial state:
The initial value for Bank is 1
The initial value for Group is 0
The initial value for ADC_old is the ADC value when entering NORMAL_MODE.

In NORMAL_MODE, the controller awaits in a loop for an event to happen.

These are the possible events:
- When a switch from SW_1 to SW_8 is pressed, we should turn off all the LEDs from 1-8, and then light up the LED corresponding to the switch (e.g. LED3 for SW_3)
  - After the switch is registered, the SWT variable should remember the number of the switch pressed (1-8).
  - A MIDI_Program_Change message is going to be sent on UART with a structure that will be defined later.
- When SW_9 is pressed it toggles the Group variable from 0 to 1, which would result in also toggling between LED_10 and LED_11.
- The potentiometer is turned (ADC value changes more than the treshold). When this happens, a MIDI_Control_Change message is sent with a structure that will be defined later.
- When SW_10 switch is pressed, it will enter the BANK_SELECT subroutine.
    - In BANK_SELECT the firmware enters a loop and listens for a press on a switch from SW_1 to SW_8. After one of the eight switches is pressed, the Bank variable is updated with the number of that switch, the segment display will show the new bank value and a MIDI_Program_Change message will be sent.
    - If SW_10 is pressed during BANK_SELECT, the BANK_SELECT subroutine ends, and NORMAL_MODE resumes.
    - During the BANK_SELECT subroutine, No MIDI_Control_Change is transmitted
- When SW_11 is pressed, the controller will enter PEDAL_MODE (described in a separate section)

## 6. Midi Message Format

### 6.1 Midi Program Change

The MIDI Program Change message has the following structure in hex format: CX YY
C - First nibble is always C and stays unchanged
X - the MIDI Channel (0-15) in hex format
YY - the Program number (0-127) in hex format

The program number is computed as follows: 64 * Group + 8 * Bank + SWT - 9.

### 6.2 Midi Control Change

The MIDI_Control_Change message has the following structure: "F0 41 aa 13 12 00 1D bb cc F7"

Bytes 1,2,4,5,6,7 and 10 are fixed always.
Byte 3 (aa) - is the MIDI Channel (0-15 in hex format)
Byte 8 (bb) - is the new ADC value (0-127 in hex format)
Byte 9 (cc) - is the checksum and is computed as 80 - (1D + bb) in hex





