# Board status LEDs

> This is an **altered version** of the DSpico firmware, not the official one.
> The original is at [LNH-team/dspico-firmware](https://github.com/LNH-team/dspico-firmware).

The DSpico board has a red and a blue LED that the stock firmware never drives,
so on a normal cartridge they stay dark for the life of the board. This branch
puts them to work.

| LED | Meaning |
| --- | --- |
| **Blue** | SD card traffic. A faint shimmer while the card is being read, a brighter blip when something is written. |
| **Red** | The firmware is stuck on the SD card and is not coming back. Stays lit until the next boot. |

Read the [limits](#what-the-leds-do-not-tell-you) before relying on either of
them. They are useful, not authoritative.

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

## How it works

`SdCard::TryBeginReadSectors` and `TryBeginWriteSectors` bump a counter
(`src/led.h`). Every transfer in the firmware goes through one of those two
functions, so the counters see all of them. `ledUpdate()` in `src/led.cpp` turns
those counters into a pin state, once per main loop pass.

Counting at the source rather than sampling the card state matters more than it
sounds. R4 mode, which is enabled by default, streams roms and writes saves
through blocking calls that start **and finish** inside a single pass of the main
loop. Anything that samples the card from that loop sees an idle card every time
and shows nothing at all for that entire mode.

A single 512 byte transfer takes tens of microseconds, far too short for an eye
to catch, so brightness carries the information rather than individual blinks:

- **Reads** are lit one pass in eight. The card is worked almost continuously
  while a game runs, so at full brightness the LED would sit solid and stop
  telling you anything. Dimmed, the shimmer tracks how hard it is being worked.
- **Writes** are lit on every pass and held far longer, so a save stands out as a
  bright blip against that shimmer.

**Red** is a fault latch. `SdCard` retries a failed block forever, which is the
right thing for a card that is having a bad moment but means a card that has come
loose spins in that retry for good, with the DS simply not responding and nothing
to see. After 64 consecutive failed blocks, or 4096 spins waiting for a card that
never answers, the red LED comes on and the blue one is turned off and stays off.
The same happens on the six paths where the firmware's own bookkeeping desyncs and
it drops into `__breakpoint()`.

Both thresholds are heuristics, not protocol limits. They are set high enough
that a card having a bad moment is not reported as dead, and low enough that a
real failure lights up quickly.

## What the LEDs do not tell you

Being straight about the gaps, because a status light you cannot trust is worse
than none:

- **Red does not mean your card is broken.** It means the firmware gave up
  waiting. A bad contact, a card pulled mid-write and a genuinely failing card
  all look the same from here.
- **Red does not catch everything.** It covers the SD paths. A hang somewhere
  else in the firmware still looks like a plain freeze.
- **Blue is not a byte counter.** It shows that traffic is happening, not how
  much. Two very different workloads can look alike.
- **The write blip can lag.** The hold counts main loop passes, and the loop only
  runs when an interrupt wakes it. After a save, a long idle gap followed by
  fresh activity can show the tail of that old write.

If you see red, please open an issue and say what you were doing. That is the
whole point of it existing.

## Building

The firmware does not build from a clone alone: `src/romData.S` embeds a
bootloader rom that is not in this repository (`roms/*.nds` is gitignored
upstream), and you have to produce it yourself. That part is unchanged from the
official firmware, so follow steps 1 to 3 of the
[official DSpico guide](https://github.com/LNH-team/dspico/blob/develop/GUIDE.md)
to get `default.nds`, then:

```sh
git clone https://github.com/rasalopa/dspico-firmware.git
cd dspico-firmware
git checkout leds

# not --recursive: that drags in a lot of pico-sdk submodules you do not need
git submodule update --init
cd pico-sdk && git submodule update --init && cd ..

cp /path/to/default.nds roms/
# DETECT_CONSOLE_TYPE is enabled by default, so this one is needed too
cp /path/to/dsimode.nds roms/

./compile.sh
```

The result is `build/DSpico.uf2`. If `compile.sh` complains about permissions,
run `chmod +x compile.sh` and retry.

To confirm you built this version and not the stock one:

```sh
picotool info build/DSpico.uf2
# description: Ntr card emulator (altered build: board status LEDs)
```

## Flashing

Same procedure as the official firmware, quoting the guide above:

1. Boot the DSpico in BOOTSEL mode. With firmware already on it, eject the micro
   SD card and connect it to a PC over USB and it enters BOOTSEL by itself.
   Otherwise hold the BOOTSEL button while connecting.
2. Copy `DSpico.uf2` to the USB drive that appears.
3. Disconnect.

## Going back

Nothing here is permanent. Flash the official `DSpico.uf2` the same way and the
board is exactly as it was, LEDs dark again. The BOOTSEL route does not depend on
the firmware working, so a bad build cannot leave you stranded.

## Tuning

At the top of `src/led.cpp`:

- `LED_READ_DUTY_SHIFT` (default `3`, one pass in 2^3) — lower it if the read
  shimmer is too faint on your board.
- `LED_WRITE_HOLD_PASSES` (default `20000`) — how long the write blip is held.
  Too low and a write looks like a read, too high and consecutive writes merge
  into one blob.
- `LED_READ_HOLD_PASSES` (default `2000`) — how long a read keeps the LED alive
  after the last transfer started.

In `src/sd/SdCard.cpp`, `kBlockFailuresBeforeFault` and `kStopSpinsBeforeFault`
control how patient the firmware is before lighting red.

All of them count main loop passes rather than milliseconds, because power saving
gates the timer clock and there is no usable wall clock in that loop.

## Disclaimer

- **This is not official DSpico firmware.** Do not report problems with it to the
  LNH team. Open an issue on this fork instead.
- It has been tested on **one board, mine**. The pinout it relies on is documented
  by the LNH team and I have no reason to expect differences between boards, but
  I can only speak for the one I have.
- It drives GPIO27 and GPIO28 as outputs at 2 mA. If your board has anything else
  wired to those pins, do not flash this.
- The fault detection changes `SdCard.cpp`, which is on the path every SD transfer
  takes. It only adds a counter and a comparison, but it is not a cosmetic change
  and you should read the diff before trusting your saves to it.
- As with the upstream firmware, this is provided as-is under the
  [zlib license](../LICENSE.txt), without warranty of any kind.

## History

Written 31 July 2026, published 1 August 2026. The `leds` branch is, in order:
claim the pins, red fault latch, blue activity, then this documentation and the
fixes that came out of reviewing all of it before publishing.
