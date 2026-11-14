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

During `PROGRAM_MODE` (section 4), the following combination is allowed:

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

#### 1.1.5 Switch actions trigger on press

All switch actions in this document are triggered by an accepted (valid) **press**, not by the release. Releases only matter for ending the lockout (1.1.2).

### 1.2 ADC (`POT`)

#### 1.2.1 Definitions

| Name | Definition |
|---|---|
| `ADC` | The latest raw 12-bit reading of `POT` (0 to 4095), right-aligned, with no scaling. |
| `ADC_old` | The raw 12-bit `ADC` value at the time of the last accepted change. |
| `ADC_STEP` | `1 << 5` (32). This is the size of one 7-bit step (4096 / 128). |
| `POT_VALUE` | `ADC_old >> 5`, giving a 7-bit value (0 to 127). |

#### 1.2.2 Behavior

1. **Initialization:** `ADC_old` is initialized from the **first valid ADC reading after power-up**, and is **re-initialized from the current `ADC` reading every time `NORMAL_MODE` is entered or resumed** (after power-up, `PROGRAM_MODE`, `BANK_SELECT` or `PEDAL_MODE`). No MIDI message is sent at these points.
2. **Change detection:** on every new reading, if `abs(ADC_old - ADC) >= ADC_STEP`:
   - `ADC_old` is set to `ADC`.
   - A `MIDI_Sysex_Pot` message is sent with the value `POT_VALUE` (message format in section 6.2).
3. Otherwise `ADC_old` is unchanged and nothing is sent.

Note: because the threshold equals one 7-bit step, every accepted change of `ADC_old` results in a `POT_VALUE` different from the previous one. No separate "value changed" check is needed.

#### 1.2.3 Scope

- The ADC behavior is **not active in `PROGRAM_MODE`** and **not active during `BANK_SELECT`**: the `POT` is not evaluated and no MIDI messages are sent.
- It is active in `NORMAL_MODE` (section 5).
- It is active in `PEDAL_MODE` (section 7) with the same behavior and limits, except while the controller waits for the pedal response (section 7.1), when the `POT` is ignored.

---

## 2. Firmware Overview

The foot controller operates in one of three modes:

- `PROGRAM_MODE`
- `NORMAL_MODE` (which includes the `BANK_SELECT` subroutine)
- `PEDAL_MODE`

### 2.1 Patch organization

- There are two groups of patches: **Group 0** and **Group 1**.
- Each group has 8 banks (**Bank 1** to **Bank 8**).
- Each bank has 8 patches (**Patch 1** to **Patch 8**).

### 2.2 Variables

| Variable | Storage | Initial value | Notes |
|---|---|---|---|
| `MIDI_Channel` | RAM, **persisted in flash** | `0` (also used if flash is empty or invalid) | Value 0 to 15. The only variable persisted across power cycles. |
| `Group` | RAM | `0` | Group number (0 or 1). |
| `Bank` | RAM | `1` | Bank number (1 to 8). |
| `SWT` | RAM | `1` | Currently selected patch number (1 to 8), i.e. the number of the last patch switch pressed. |
| `ADC_old` | RAM | First valid ADC reading; re-initialized every time `NORMAL_MODE` is entered or resumed (see section 1.2) | |
| `b1` to `b8` | RAM | `0` | Binary values (0 or 1) used in `PEDAL_MODE` (section 7). Set to 0 when `PEDAL_MODE` is entered and when it is left. |

Rules:

- Only `MIDI_Channel` is persisted. All other variables live in RAM.
- `Group`, `Bank` and `SWT` are set to their initial values **once, at power-up**. They are **not** re-initialized when entering `NORMAL_MODE` again (for example when returning from `PEDAL_MODE`).

### 2.3 Group indication

`LED_10` and `LED_11` display the current group. Exactly one of them is lit at any moment, in every mode:

| `Group` | `LED_10` | `LED_11` |
|---|---|---|
| 0 | ON | OFF |
| 1 | OFF | ON |

### 2.4 Bank display

In `NORMAL_MODE` the 7-segment display always shows the current `Bank` digit (1 to 8). Segment patterns:

| Digit | Segments ON |
|---|---|
| 1 | B, C |
| 2 | A, B, D, E, G |
| 3 | A, B, C, D, G |
| 4 | B, C, F, G |
| 5 | A, C, D, F, G |
| 6 | A, C, D, E, F, G |
| 7 | A, B, C |
| 8 | A, B, C, D, E, F, G |

In `PEDAL_MODE` the display shows the letter `P` (segments A, B, E, F, G).

The decimal point (`SEG_DP`) is OFF in `NORMAL_MODE` and in `PEDAL_MODE`. It is ON during `PROGRAM_MODE` and during `BANK_SELECT`.

---

## 3. Power ON

At power-up the variables are set to their initial values (section 2.2). The controller then enters `PROGRAM_MODE` if the entry condition in section 1.1.4 is met (`SW_1` and `SW_2` both held and debounced). Otherwise it enters `NORMAL_MODE`.

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

The channel is selected when a switch `SW_n` (n = 1 to 8) is accepted (valid press, section 1.1.5):

| Situation when `SW_n` is accepted | Channel selected |
|---|---|
| `SW_10` is **not** held | `n - 1` (channel 0 to 7) |
| `SW_10` was pressed first and is **still held** (combination, section 1.1.3) | `n + 7` (channel 8 to 15) |

Examples: `SW_1` gives channel 0, `SW_8` gives channel 7, `SW_10` + `SW_1` gives channel 8, `SW_10` + `SW_8` gives channel 15.

On acceptance:

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
- It is written **only** when a channel selection is made (section 4.3), before the exit blinks start.
- At power-up the value is read from flash. If flash is empty or the stored value is invalid (not 0 to 15), `MIDI_Channel` is `0`.
- Flash erase/write may block the CPU for a short time. This is acceptable at this point.

---

## 5. NORMAL_MODE

### 5.1 Entering or resuming NORMAL_MODE

Every time `NORMAL_MODE` is entered or resumed (after power-up, `PROGRAM_MODE`, `BANK_SELECT` or `PEDAL_MODE`):

- `Group`, `Bank` and `SWT` keep their current values (they are initialized only at power-up, section 2.2).
- `LED_1` to `LED_8`: only the LED matching `SWT` is ON (e.g. `SWT` = 3 lights `LED_3`), all others are OFF. At power-up this means `LED_1` is ON.
- `LED_10` / `LED_11` show the group (section 2.3).
- The 7-segment display shows the current `Bank` digit with `SEG_DP` OFF (section 2.4).
- `ADC_old` is re-initialized from the current `ADC` reading (section 1.2.2).
- **No MIDI message is sent** by entering or resuming `NORMAL_MODE` itself. (The `MIDI_Program_Change` sent when `PEDAL_MODE` is left is part of the `PEDAL_MODE` exit sequence, section 7.4.)

### 5.2 Events

In `NORMAL_MODE` the firmware waits in a loop for one of the following events. All switch events follow the rules in section 1.1.

| Event | Action |
|---|---|
| `SW_1` to `SW_8` pressed | See 5.2.1 |
| `SW_9` pressed | See 5.2.2 |
| `POT` moved by at least one 7-bit step | `MIDI_Sysex_Pot` is sent, as defined in section 1.2. |
| `SW_10` pressed | The controller enters the `BANK_SELECT` subroutine (section 5.3). |
| `SW_11` pressed | The controller enters `PEDAL_MODE` (section 7). |

#### 5.2.1 Patch switch (`SW_1` to `SW_8`)

When `SW_n` is accepted:

1. `LED_1` to `LED_8` are all turned OFF, then `LED_n` is turned ON.
2. `SWT` is set to `n`.
3. A `MIDI_Program_Change` message is sent (section 6.1).

Pressing the switch of the patch that is already selected behaves the same way: the message is sent again.

#### 5.2.2 Group switch (`SW_9`)

When `SW_9` is accepted:

1. `Group` is toggled (0 becomes 1, 1 becomes 0).
2. `LED_10` / `LED_11` are updated (section 2.3).
3. A `MIDI_Program_Change` message is sent (section 6.1), using the new `Group` and the current `Bank` and `SWT`.

### 5.3 BANK_SELECT subroutine

Entered when `SW_10` is accepted in `NORMAL_MODE`.

On entry:

- `SEG_DP` turns ON. The 7-segment display keeps showing the current `Bank` digit.
- `LED_1` to `LED_8` stay unchanged (still showing `SWT`).
- The `SW_10` press that started `BANK_SELECT` must be released (together with all other switches) before another press is accepted, according to the lockout rule in section 1.1.2.

While in `BANK_SELECT`:

- `SW_1` to `SW_8`: `Bank` is set to the number of the pressed switch, the 7-segment display shows the new `Bank` digit, and a `MIDI_Program_Change` message is sent (section 6.1) using the current `Group` and `SWT`. `BANK_SELECT` then ends.
- `SW_10`: `BANK_SELECT` ends without changing `Bank` and without sending any message.
- `SW_9` and `SW_11` are ignored.
- The `POT` is not evaluated, so no `MIDI_Sysex_Pot` message is sent (section 1.2.3).

When `BANK_SELECT` ends, `SEG_DP` turns OFF, `ADC_old` is re-initialized from the current `ADC` reading, and `NORMAL_MODE` resumes (section 5.1). The lockout from section 1.1.2 carries over: the switch that ended `BANK_SELECT` must be released before the next press is accepted.

---

## 6. MIDI Message Format

All messages are sent on the UART described in `schematic.md` (31250 baud, 8N1).

### 6.1 MIDI_Program_Change

Standard MIDI Program Change message, 2 bytes: `CX YY` (hex).

| Byte | Value |
|---|---|
| `CX` | Status byte. `C` is always the first nibble. `X` is `MIDI_Channel` (0 to 15) in hex. |
| `YY` | Program number (0 to 127) in hex. |

The program number is computed as:

```
program = 64 * Group + 8 * Bank + SWT - 9
```

This gives 0 (`Group` 0, `Bank` 1, `SWT` 1) to 127 (`Group` 1, `Bank` 8, `SWT` 8).

Examples:

| `MIDI_Channel` | `Group` | `Bank` | `SWT` | Program | Message |
|---|---|---|---|---|---|
| 0 | 0 | 1 | 1 | 0 | `C0 00` |
| 0 | 0 | 2 | 3 | 10 | `C0 0A` |
| 3 | 1 | 1 | 1 | 64 | `C3 40` |
| 15 | 1 | 8 | 8 | 127 | `CF 7F` |

### 6.2 MIDI_Sysex_Pot

This is a Roland System Exclusive (SysEx) message. It is **not** a MIDI Control Change message. The bytes were taken from a MIDI capture of the real unit.

Structure (10 bytes, hex): `F0 41 aa 13 12 00 1D bb cc F7`

| Byte | Value | Meaning |
|---|---|---|
| 1 | `F0` | Fixed. Start of SysEx. |
| 2 | `41` | Fixed. Roland manufacturer ID. |
| 3 | `aa` | `MIDI_Channel` (0 to 15) in hex. |
| 4 | `13` | Fixed. |
| 5 | `12` | Fixed. |
| 6 | `00` | Fixed. |
| 7 | `1D` | Fixed. |
| 8 | `bb` | `POT_VALUE` (0 to 127) in hex, see section 1.2. |
| 9 | `cc` | Checksum, see below. |
| 10 | `F7` | Fixed. End of SysEx. |

Checksum (Roland, covers the address bytes `00 1D` and the data byte `bb`, always a 7-bit result):

```
cc = (0x80 - ((0x1D + bb) & 0x7F)) & 0x7F
```

Test vectors:

| `bb` | `cc` | Full message (channel 0) |
|---|---|---|
| `00` | `63` | `F0 41 00 13 12 00 1D 00 63 F7` |
| `40` | `23` | `F0 41 00 13 12 00 1D 40 23 F7` |
| `63` | `00` | `F0 41 00 13 12 00 1D 63 00 F7` |
| `7F` | `64` | `F0 41 00 13 12 00 1D 7F 64 F7` |

### 6.3 Transmission

- Messages are queued and sent in order. Sending must not block the main loop.
- At 31250 baud a byte takes about 320 µs, so a `MIDI_Program_Change` takes about 0.64 ms and a `MIDI_Sysex_Pot` about 3.2 ms.
- Reception on the UART is also non-blocking (interrupt or DMA driven). Bytes received outside the response wait window (section 7.1) are discarded, in every mode.

### 6.4 MIDI_Sysex_Pedal_Request

Roland SysEx request, 11 bytes (hex): `F0 41 nn 13 11 00 00 00 32 4E F7`

- `nn` is `MIDI_Channel` (0 to 15) in hex.
- All other bytes are fixed. The checksum `4E` covers bytes 6 to 9 (`00 00 00 32`) and is always the same.

The unit answers with a `MIDI_Sysex_Pedal_Response` (section 6.5).

### 6.5 MIDI_Sysex_Pedal_Response

A SysEx message of exactly **59 bytes** (numbering from 1) sent by the unit:

| Byte(s) | Meaning |
|---|---|
| 1 to 7 | Header: `F0 41 nn 13 12 00 00`, with `nn` equal to `MIDI_Channel`. |
| 8 | Only the lower nibble is used (`b8` to `b5`). The upper nibble is always 0. |
| 9 | Only the lower nibble is used (`b4` to `b1`). The upper nibble is always 0. |
| 10 to 57 | Other data, ignored by the firmware (but included in the checksum). |
| 58 | Checksum (Roland), computed over bytes 6 to 57. |
| 59 | `F7`. End of SysEx. |

Mapping to `b1` to `b8`:

```
B = ((byte8 & 0x0F) << 4) | (byte9 & 0x0F)
b_n = bit (n - 1) of B        (so b8 is the top bit, b1 is the bottom bit)
```

Equivalently: the lower nibble of byte 8 holds `b8 b7 b6 b5` (most significant bit first), and the lower nibble of byte 9 holds `b4 b3 b2 b1`.

Example (abbreviated), channel 0:

```
F0 41 00 13 12 00 00 01 08 ...(48 bytes)... 4A F7
```

- Byte 8 is `01`: lower nibble `0001`, so `b8 = 0`, `b7 = 0`, `b6 = 0`, `b5 = 1`.
- Byte 9 is `08`: lower nibble `1000`, so `b4 = 1`, `b3 = 0`, `b2 = 0`, `b1 = 0`.
- Result: `b8..b1 = 0 0 0 1 1 0 0 0` (`B = 0x18`).

Rules for accepting a response are in section 7.1.

### 6.6 MIDI_Sysex_Pedal_Change

Roland SysEx message, 11 bytes (hex): `F0 41 nn 13 12 00 00 bb cc dd F7`

| Byte | Value | Meaning |
|---|---|---|
| 1 | `F0` | Fixed. |
| 2 | `41` | Fixed. |
| 3 | `nn` | `MIDI_Channel` (0 to 15) in hex. |
| 4 | `13` | Fixed. |
| 5 | `12` | Fixed. |
| 6 | `00` | Fixed. |
| 7 | `00` | Fixed. |
| 8 | `bb` | `(B >> 4) & 0x0F`, i.e. `b8 b7 b6 b5`. The upper nibble is 0. |
| 9 | `cc` | `B & 0x0F`, i.e. `b4 b3 b2 b1`. The upper nibble is 0. |
| 10 | `dd` | Checksum (Roland). |
| 11 | `F7` | Fixed. |

`B` is built from the current `b1` to `b8` as in section 6.5 (reverse mapping: `B = sum of b_n << (n - 1)`).

Checksum (covers bytes 6 to 9, always a 7-bit result):

```
dd = (0x80 - ((0x00 + 0x00 + bb + cc) & 0x7F)) & 0x7F
```

Test vectors (channel 0):

| `b8..b1` | `bb` | `cc` | `dd` | Message |
|---|---|---|---|---|
| `00000000` | `00` | `00` | `00` | `F0 41 00 13 12 00 00 00 00 00 F7` |
| `10011000` | `09` | `08` | `6F` | `F0 41 00 13 12 00 00 09 08 6F F7` |
| `11011000` | `0D` | `08` | `6B` | `F0 41 00 13 12 00 00 0D 08 6B F7` |
| `11111111` | `0F` | `0F` | `62` | `F0 41 00 13 12 00 00 0F 0F 62 F7` |

Worked example. The response from section 6.5 gave `b8..b1 = 0 0 0 1 1 0 0 0`:

1. `SW_8` is pressed. `b8` toggles to 1 and `LED_8` turns ON. Now `b8..b1 = 1 0 0 1 1 0 0 0`, so `bb = 09`, `cc = 08` and the message is `F0 41 00 13 12 00 00 09 08 6F F7`.
2. `SW_7` is then pressed. `b7` toggles to 1 and `LED_7` turns ON. Now `b8..b1 = 1 1 0 1 1 0 0 0`, so `bb = 0D`, `cc = 08` and the message is `F0 41 00 13 12 00 00 0D 08 6B F7`.

---

## 7. PEDAL_MODE

Entered when `SW_11` is accepted in `NORMAL_MODE` (section 5.2). In this mode `b1` to `b8` mirror the state of eight functions on the unit (`LED_1` to `LED_8`), which the user toggles with `SW_1` to `SW_8`.

### 7.1 Entering PEDAL_MODE

On entry:

- `LED_9` turns ON.
- The 7-segment display shows the letter `P` (segments A, B, E, F, G), with `SEG_DP` OFF.
- `LED_10` / `LED_11` keep showing the group (section 2.3).
- `b1` to `b8` are set to 0, and `LED_1` to `LED_8` mirror `b1` to `b8` at all times during `PEDAL_MODE` (1 = ON, 0 = OFF). They are therefore all OFF at this point.
- A `MIDI_Sysex_Pedal_Request` is sent (section 6.4).
- `Group`, `Bank` and `SWT` are unchanged.

**Response wait window:**

- The window starts when the request has been fully transmitted. It ends when a **valid** response is received, or after **120 ms**, whichever comes first.
- During the window, all switches and the `POT` are ignored. The `SW_11` press that started `PEDAL_MODE` must also be released before another press is accepted (section 1.1.2).
- A response is accepted only if it:
  - is exactly 59 bytes long,
  - has the header `F0 41 nn 13 12 00 00` with `nn` equal to `MIDI_Channel`, and
  - has a valid checksum.
- Anything else is discarded and the window continues until a valid response arrives or the timeout expires.
- When a valid response arrives, `b1` to `b8` are set from it (section 6.5) and `LED_1` to `LED_8` are updated.
- **On timeout:** `b1` to `b8` stay at 0, `LED_1` to `LED_8` stay OFF, nothing is sent, and there is no retry. See section 8.
- Bytes received after the window ends (for example a late response) are discarded.

### 7.2 Patch switches (`SW_1` to `SW_8`)

After the wait window, when `SW_n` (n = 1 to 8) is accepted:

1. `b_n` is toggled.
2. `LED_n` toggles to match `b_n`.
3. A `MIDI_Sysex_Pedal_Change` message is sent (section 6.6) built from the current `b1` to `b8`.

### 7.3 Potentiometer

Changing the `POT` triggers a `MIDI_Sysex_Pot` message with the same behavior and limits as in `NORMAL_MODE` (section 1.2), except during the response wait window.

### 7.4 Leaving PEDAL_MODE (`SW_11`)

When `SW_11` is accepted (after the wait window):

1. `b1` to `b8` are set to 0.
2. `LED_1` to `LED_8` are turned OFF.
3. `LED_9` is turned OFF.
4. A `MIDI_Program_Change` message is sent (section 6.1) using the current `Group`, `Bank` and `SWT` (the previously selected patch). This message is part of leaving `PEDAL_MODE`.
5. The controller returns to `NORMAL_MODE`, resuming as described in section 5.1 (the LED for `SWT` lights up, the display shows `Bank`, `ADC_old` is re-initialized).

The lockout from section 1.1.2 carries over: the `SW_11` press must be released before the next press is accepted.

### 7.5 Other switches

`SW_9` and `SW_10` are ignored in `PEDAL_MODE`.

---

## 8. Known Issues and Future TBD

Items below are known limitations of this specification. They are accepted for now and may be revisited in a later stage.

1. **No pedal response (timeout).** If the unit does not answer the `MIDI_Sysex_Pedal_Request` within 120 ms, `b1` to `b8` stay at 0 and nothing else happens (no retry, no error indication). The first switch press then sends a `MIDI_Sysex_Pedal_Change` built from all zeros plus the toggled bit, which overwrites the real state of the unit. Possible future improvements: retry, an error indication, or blocking switch presses until a valid response has been received.
