#ifdef ENABLE_STATUS_LEDS

#include "common.h"
#include "led.h"
#include "hardware/gpio.h"

// How long the blue LED is held on after a write, counted in main loop passes.
// There is no usable wall clock here: power saving gates the timer clock, so
// everything below counts passes instead of milliseconds. Too low and a write
// looks like a read, too high and consecutive writes merge into one long blob.
#define LED_WRITE_HOLD_PASSES   20000u

// How long a read keeps the LED alive after the last one started. Only has to
// bridge the gap between two transfers of a game that is streaming, so it is far
// shorter than the write hold - that difference is what makes a save stand out.
#define LED_READ_HOLD_PASSES    2000u

// Reads are dimmed by lighting the LED on only one pass in 2^N. A running game
// pulls its rom off the card almost continuously, so at full brightness the LED
// would sit solid and stop carrying any information. Writes stay at full duty.
#define LED_READ_DUTY_SHIFT     3u

// Counters live outside the module so the notify helpers can stay inline.
volatile uint32_t gLedSdReadEvents = 0;
volatile uint32_t gLedSdWriteEvents = 0;

static u32 sSeenReadEvents = 0;
static u32 sSeenWriteEvents = 0;
static u32 sReadHold = 0;
static u32 sWriteHold = 0;
static u32 sPass = 0;
static bool sFaulted = false;

void ledInit(void)
{
    // Set the level before the direction so the pin never glitches a flash of
    // light at boot, and keep the 2 mA drive the other outputs here use.
    gpio_set_drive_strength(PIN_LED_R, GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(PIN_LED_B, GPIO_DRIVE_STRENGTH_2MA);
    gpio_disable_pulls(PIN_LED_R);
    gpio_disable_pulls(PIN_LED_B);
    gpio_put(PIN_LED_R, false);
    gpio_put(PIN_LED_B, false);
    gpio_set_dir(PIN_LED_R, GPIO_OUT);
    gpio_set_dir(PIN_LED_B, GPIO_OUT);
}

void ledUpdate(void)
{
    // A latched fault owns both LEDs from here on, so a burst of card traffic
    // cannot paint over the one signal that matters.
    if (sFaulted)
    {
        return;
    }

    uint32_t reads = gLedSdReadEvents;
    uint32_t writes = gLedSdWriteEvents;
    if (writes != sSeenWriteEvents)
    {
        sSeenWriteEvents = writes;
        sWriteHold = LED_WRITE_HOLD_PASSES;
    }
    if (reads != sSeenReadEvents)
    {
        sSeenReadEvents = reads;
        sReadHold = LED_READ_HOLD_PASSES;
    }

    sPass++;
    bool dimTick = (sPass & ((1u << LED_READ_DUTY_SHIFT) - 1u)) == 0u;
    gpio_put(PIN_LED_B, sWriteHold != 0 || (sReadHold != 0 && dimTick));

    if (sWriteHold != 0)
    {
        sWriteHold--;
    }
    if (sReadHold != 0)
    {
        sReadHold--;
    }
}

void ledPrepareForSleep(void)
{
    if (sFaulted)
    {
        return;
    }
    gpio_put(PIN_LED_B, false);
}

void ledSignalError(void)
{
    if (sFaulted)
    {
        return;
    }
    sFaulted = true;
    // Blue off first: a fault taken while it happened to be lit would otherwise
    // strand it on next to the red one and muddle the signal.
    gpio_put(PIN_LED_B, false);
    gpio_put(PIN_LED_R, true);
}

#endif // ENABLE_STATUS_LEDS
