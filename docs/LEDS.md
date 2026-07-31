# Board status LEDs

> This is an **altered version** of the DSpico firmware, not the official one.
> The original is at [LNH-team/dspico-firmware](https://github.com/LNH-team/dspico-firmware).

The DSpico board has a red and a blue LED that the stock firmware never drives,
so on a normal cartridge they stay dark for the life of the board. This branch
puts them to work.

| LED | Meaning |
| --- | --- |
| **Blue** | SD card traffic. A faint shimmer while a game streams its rom, a bright blip when the game saves. |
| **Red** | The firmware hit a fault it cannot recover from. Stays lit until the next boot. |

## Where the pins come from

They are documented hardware, not a guess. The pinout table in
[LNH-team/dspico-hardware](https://github.com/LNH-team/dspico-hardware) lists
both, and the feature list there says "Two LEDs (red and blue)":

| | Signal | GPIO | Pin |
| --- | --- | --- | --- |
| Red LED | `LED_R` | GPIO27 | 39 |
| Blue LED | `LED_B` | GPIO28 | 40 |

Both are active high. Nothing here changes the board, it only drives pins that
were already there and already wired to LEDs.

## Reading them

**Blue.** A single 512 byte transfer takes tens of microseconds, far too short
for an eye to catch, so brightness is what carries the information, not
individual blinks:

- **Reads** are lit one main-loop pass in eight. A running game streams its rom
  off the card almost continuously, so at full brightness the LED would just sit
  solid and tell you nothing. Dimmed, you get a shimmer that tracks how hard the
  card is being worked.
- **Writes** are lit on every pass, with a short tail afterwards. Eight times the
  duty of a read, so a save stands out clearly against the shimmer.

**Red.** Every SD transfer failure in the game code path ends in
`__breakpoint()`. With no debugger attached that is a silent hard lock: the
cartridge stops answering and the DS just sits there. The red LED latches on
before that happens, so instead of "it froze" you get a visible signal that the
firmware died on an SD transfer. Nothing clears it on purpose. If you see it,
please open an issue and say what you were doing.

## Building

The `pico-sdk` is a submodule, so it has to be pulled in before the first build.

```sh
git clone https://github.com/rasalopa/dspico-firmware.git
cd dspico-firmware
git checkout leds
git submodule update --init
./compile.sh
```

The result is `build/DSpico.uf2`. If `compile.sh` complains about permissions,
run `chmod +x compile.sh` and retry. For the toolchain itself, follow the
official [DSpico guide](https://github.com/LNH-team/dspico/blob/main/GUIDE.md),
nothing here changes those requirements.

## Flashing

Same procedure as the official firmware, quoting the guide above:

1. Boot the DSpico in BOOTSEL mode. With the official firmware already on it,
   eject the micro SD card and connect it to a PC over USB and it enters BOOTSEL
   by itself. Otherwise hold the BOOTSEL button while connecting.
2. Copy `DSpico.uf2` to the USB drive that appears.
3. Disconnect.

## Going back

Nothing here is permanent. Flash the official `DSpico.uf2` the same way and the
board is exactly as it was, LEDs dark again. The BOOTSEL route does not depend
on the firmware working, so a bad build cannot leave you stranded.

## Tuning

Two knobs at the top of `src/main.cpp`:

- `LED_READ_DUTY_SHIFT` (default `3`, one pass in 2^3) — lower it if the read
  shimmer is too faint on your board.
- `LED_WRITE_HOLD_PASSES` (default `20000`) — how long the write blip is held.
  Too low and a write looks like a read, too high and consecutive writes merge
  into one long blob.

Both are counted in main-loop passes rather than milliseconds, because power
saving gates the timer clock and there is no usable wall clock in that loop.

## Disclaimer

- **This is not official DSpico firmware.** Do not report problems with it to the
  LNH team. Open an issue on this fork instead.
- It has been tested on **one board, mine**. The pinout it relies on is
  documented by the LNH team and I have no reason to expect differences between
  boards, but I can only speak for the one I have.
- It drives GPIO27 and GPIO28 as outputs at 2 mA. If your board has anything else
  wired to those pins, do not flash this.
- As with the upstream firmware, this is provided as-is under the
  [zlib license](../LICENSE.txt), without warranty of any kind.

## History

Written 31 July 2026, published 1 August 2026. The three commits on the `leds`
branch are, in order: claim the pins, red fault latch, blue activity.
