# GP-8 MIDI Foot Controller (STM32F401RCT6)

Firmware for a MIDI foot controller for the Roland GP-8.

- Hardware: [docs/schematic.md](docs/schematic.md)
- Behavior: [docs/firmware_spec.md](docs/firmware_spec.md) (the firmware implements this document)

## Layout

| Path | Purpose |
|---|---|
| `src/app.c` | State machine: PROGRAM_MODE, NORMAL_MODE, BANK_SELECT, PEDAL_MODE |
| `src/midi.c` | MIDI message builders, Roland checksum, pedal response parser |
| `src/switches.c` | 30 ms debounce, first-press-wins lockout, SW_10 combination |
| `src/hal.h` | Hardware interface used by the logic above |
| `src/hal_stm32.c` | STM32F401 implementation (bare CMSIS registers, no HAL library) |
| `tests/` | Host-side unit tests with a mock HAL (no hardware needed) |

`app.c`, `midi.c` and `switches.c` never touch hardware, so they run unchanged in the host tests.

## Run the tests (host)

```
cd tests
make
```

The tests check the message test vectors from the spec (checksums, program numbers, the real pedal response capture) and walk through every mode.

## Build and flash (PlatformIO)

```
pio run
pio run -t upload      # USB DFU
```

Because PA13/PA14 (SWD) are used as LED outputs, SWD cannot be used. Put the board in DFU mode (BOOT0 high, reset), then upload.

## Implementation choices (not in the spec)

- **Clock:** internal HSI 16 MHz, no PLL. This does not depend on the board's crystal, and USART1 reaches exactly 31250 baud.
- **ADC:** ADC1 channel 8, 12-bit, 480-cycle sampling, one software-triggered conversion per main-loop pass, no averaging.
- **MIDI_Channel storage:** flash sector 3 (`0x0800C000`, 16 KB), one 32-bit word (magic + channel). The program must stay below 48 KB (`board_upload.maximum_size` enforces this). Writing erases the sector, which stalls the CPU for a few hundred ms; this happens once per channel selection.
- **PB7 (MIDI RX):** internal pull-up enabled so the line idles high if nothing is connected.
- **Startup:** the first 30 ms after power-up are used to confirm that SW_1 and SW_2 are both held (PROGRAM_MODE entry).
