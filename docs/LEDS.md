# Board status LEDs

> This is an **altered version** of the DSpico firmware, not the official one.
> The original is at [LNH-team/dspico-firmware](https://github.com/LNH-team/dspico-firmware).

The DSpico board has a red and a blue LED that the stock firmware never drives,
so on a normal cartridge they stay dark for the life of the board. This branch
puts them to work.

| LED | Meaning |
| --- | --- |
| **Blue** | SD card traffic. A faint shimmer while the card is being read, a brighter blip when something is written. |
| **Red** | The firmware stopped on a fault it cannot come back from. Stays lit until the next boot. **It does not detect a failing or loose SD card** - see [the limits](#what-the-leds-do-not-tell-you). |

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

`SdCard`'s state machine bumps a counter (`src/led.h`) where it starts each read
and each write (`StateReadBegin` and `StateWriteBegin` in `src/sd/SdCard.cpp`).
Every transfer in the firmware passes through one of those two, so the counters
see all of them. `ledUpdate()` in `src/led.cpp` turns those counters into a pin
state, once per main loop pass.

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

The reads the firmware makes on its own at power-on, to mount the card, do not
light the LED: `ledStart()` marks them as seen just before the main loop starts.
They used to arm the read hold on the first pass, and the hold then sat there
until wake-ups spent it. Outside a console the only thing that wakes the loop is
a finger on the floating bus pins, so blue lit with no transfer at all, stayed lit
while the finger was there, and went dark for good once the hold ran out.

**Red** is lit by the HardFault handler. Upstream handles the six internal
inconsistencies in `src/ntrCardRomGameSd.cpp` - a transfer started while the
firmware believes the card is idle and it is not - by dropping into
`__breakpoint()`, which with no debugger attached escalates to a HardFault and a
silent hard lock. This build replaces the SDK's default handler with one that
turns blue off, turns red on and parks the core, so that lock has something
visible attached to it, and so does any other fault that ends up there.

Why the handler and not a call on those six paths, which is what an earlier
version of this branch did: the card command handlers live in SCRATCH_Y, the same
4 KB bank the core 0 stack grows down into, with only a few hundred bytes to
spare - upstream's linker layout does not guard that boundary. A call from those
handlers into flash needs a long-branch veneer in that bank, and the veneers sit
at its very top, right under the stack. The LED version that called from there
grew the bank by 120 bytes, a quarter of that margin. Whether that alone broke
anything was not isolated on hardware; it is kept out because the margin is not
this feature's to spend. This build adds no bytes to SCRATCH_Y: the SD counters
live in `SdCard.cpp`'s state machine, which is in flash, and red is lit from the
fault handler.

### What broke the USB examples

With the first builds of this branch, the DS side USB examples
([dspico-usb-examples](https://github.com/LNH-team/dspico-usb-examples)) never
showed up on the PC, while the stock firmware works. Bisected on hardware, one
change at a time: the same build with `ledUpdate` and `ledPrepareForSleep`
removed from the main loop enumerates, and so does the build with those two
placed in RAM (`__time_critical_func`), which is what this branch does now. The
build that ran them from flash every pass, while the DS polls the cartridge for
USB events, is the one that never enumerated. The exact mechanism is not pinned
down and the code says so.

This is a narrow signal on purpose. An earlier version of this branch also tried
to light red from the retry loops inside `SdCard.cpp`, to cover a card that had
come loose. Reviewing it before publishing showed that bounding those loops by a
spin count lights red on a healthy card during a perfectly normal save - the busy
period after a multi-block write is milliseconds, and any iteration count small
enough to be useful is tens of microseconds. A fault light that cries wolf on
every save is worse than one that stays dark, so that part was dropped rather
than shipped half-right.

## What the LEDs do not tell you

Being straight about the gaps, because a status light you cannot trust is worse
than none:

- **Red does not detect a bad SD card, and that is the big one.** If a card comes
  loose or starts failing, the firmware wedges in an unbounded retry inside
  `SdCard.cpp` that red does not watch. The DS stops responding and both LEDs stay
  as they were. This is the most likely real world failure and it is exactly the
  case this does not cover. Do not read a dark red LED as "the card is fine".
- **Red covers faults, not hangs.** Anything that ends in a HardFault lights it;
  a loop that never returns, such as the retry above, looks like a plain freeze.
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

# Read the note below before skipping these two: without them the cart stops
# booting on an unmodified DSi or 3DS.
cp /path/to/WRFUTester_v0.60.nds roms/dsimode.nds
cp /path/to/uartBufv060.bin data/

./compile.sh
```

The result is `build/DSpico.uf2`. If `compile.sh` complains about permissions,
run `chmod +x compile.sh` and retry.

> [!IMPORTANT]
> **If you use the cart on a DSi or a 3DS without CFW, you need the two extra
> files.** The stock firmware ships them, so a build made from `default.nds`
> alone is a downgrade for those consoles: they refuse the homebrew bootloader
> and show a Nintendo error, while a DS and a CFW console keep working and hide
> the problem. This bit me on my own New 3DS.
>
> The two files come from step 4 of the
> [official guide](https://github.com/LNH-team/dspico/blob/develop/GUIDE.md):
> `roms/dsimode.nds` is the WRFU Tester v0.60 rom (sha1
> `2d65fb7a0c62a4f08954b98c95f42b804fccfd26`) and `data/uartBufv060.bin` is the
> [Wrfuxxed](https://github.com/LNH-team/dspico-wrfuxxed) payload built with
> BlocksDS and then DLDI patched with the
> [DSpico DLDI](https://github.com/LNH-team/dspico-dldi):
>
> ```sh
> dlditool /path/to/DSpico.dldi uartBufv060.bin
> ```
>
> `CMakeLists.txt` picks both up on its own: `DETECT_CONSOLE_TYPE` turns on when
> both roms are present and `DSPICO_ENABLE_WRFUXXED` when the payload is in
> `data/`. Nothing to edit, and a build without them still compiles.

To confirm you built this version and not the stock one:

```sh
picotool info build/DSpico.uf2
# description: Ntr card emulator (board status LEDs)
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

- `LED_READ_DUTY_SHIFT` (default `3`, one pass in 2^3): lower it if the read
  shimmer is too faint on your board.
- `LED_WRITE_HOLD_PASSES` (default `20000`): how long the write blip is held.
  Too low and a write looks like a read, too high and consecutive writes merge
  into one blob.
- `LED_READ_HOLD_PASSES` (default `2000`): how long a read keeps the LED alive
  after the last transfer started.

All three count main loop passes rather than milliseconds, because power saving
gates the timer clock and there is no usable wall clock in that loop. There is
nothing to tune for the red LED: it fires on a condition, not a threshold.

## Disclaimer

- **This is not official DSpico firmware.** Do not report problems with it to the
  LNH team. Open an issue on this fork instead.
- It has been tested on **four boards**: mine, on a DS Lite, a 3DS with CFW and a
  New 3DS without, and three more from people who tried an earlier build. The
  28 September changes have only been tried on mine so far. The pinout it
  relies on is documented by the LNH team.
- It drives GPIO27 and GPIO28 as outputs at 2 mA. If your board has anything else
  wired to those pins, do not flash this.
- `src/sd/SdCard.cpp` gains one counter increment in each of the two functions
  that start a transfer, and the include that declares them. That is the whole of
  the change to the SD path; `SdCard.h` is untouched.
- One change is not about the LEDs: `CEB` and `CS2`, the two card selects, are
  pulled up instead of left floating, as the upstream firmware had them until
  `913185e` disabled its pulls. A console drives both lines, so inside one the
  card works the same. On USB power outside a console, a finger resting on the
  contacts no longer wakes the firmware; putting it down or lifting it still can,
  for a moment.
- As with the upstream firmware, this is provided as-is under the
  [zlib license](../LICENSE.txt), without warranty of any kind.

## History

Written 31 July 2026, published 1 August 2026. At publication the `leds` branch
was, in order: claim the pins, red fault latch, blue activity, then this
documentation and the fixes that came out of reviewing all of it before
publishing.

28 September 2026: the boot mount no longer lights blue. An earlier version of
this page said the flicker outside a console was SD reads started by noise on
the bus; a build that only leaves the boot mount out stays dark under the same
finger, so it was the LED's own hold and not the card. The same day the two card
selects went back to being pulled up. Together, the two changes were tested on
my board only, in a DS Lite, a 3DS with CFW and a New 3DS without CFW.
