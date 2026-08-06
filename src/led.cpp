#ifdef ENABLE_STATUS_LEDS

#include "common.h"
#include "led.h"
#include "hardware/gpio.h"

// Power saving gates the timer clock, so the holds below count main loop passes
// rather than milliseconds. A write is held far longer than a read, and that
// difference is what makes a save stand out from a game streaming its rom.
#define LED_WRITE_HOLD_PASSES   20000u
#define LED_READ_HOLD_PASSES    2000u

// Reads light the LED on one pass in 2^N. At full duty a running game keeps it
// solid and it stops carrying any information. Writes stay at full duty.
#define LED_READ_DUTY_SHIFT     3u

// Not static, so the notify helpers in led.h can stay inline at the call sites.
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
    // Level before direction, so the pin cannot glitch a flash of light at boot.
    // 2 mA matches the drive strength the other outputs here use.
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
    // A latched fault owns both LEDs, so card traffic cannot paint over it.
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
    // Blue off first, or a fault taken while it was lit strands both on.
    gpio_put(PIN_LED_B, false);
    gpio_put(PIN_LED_R, true);
}

#endif // ENABLE_STATUS_LEDS
