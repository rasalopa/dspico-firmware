#pragma once
#include <stdint.h>

// Deliberately does NOT include common.h: that pulls in SdCard.h, which includes
// this header back, and the notify helpers would then be used before declaration.

// Bumped once every time an SD transfer is started, from BOTH the interrupt
// driven path and the blocking one (see SdCard::TryBegin*Sectors). Counting at
// the source is what makes the blocking path visible at all: an r4 mode rom read
// or a FatFs write begins AND finishes inside one main loop pass, so sampling
// the card state from that loop never sees it happen.
//
// An increment can in principle be lost if two contexts race here. The cost of
// that is one missed blink, which is not worth an interrupt disable in code this
// hot.
extern volatile uint32_t gLedSdReadEvents;
extern volatile uint32_t gLedSdWriteEvents;

/// @brief Records that a read has started. Safe from any context: one add, no
///        branch, no pin access.
static inline void ledNotifySdRead(void) { gLedSdReadEvents++; }

/// @brief Records that a write has started. See ledNotifySdRead.
static inline void ledNotifySdWrite(void) { gLedSdWriteEvents++; }

/// @brief Claims the two LED pins and parks both dark.
void ledInit(void);

/// @brief Drives the blue LED from the counters above. Call once per main loop
///        pass. Does nothing once a fault has been latched: from then on the
///        LEDs report the fault and nothing else.
void ledUpdate(void);

/// @brief Clears the blue LED before the main loop sleeps.
///
/// This has to happen every pass. The loop only runs when an interrupt wakes it,
/// so a level left set here stays set for as long as the card bus is quiet, and
/// the holds cannot count down either. Leaving it lit through __wfi to make
/// writes more obvious was tried and stranded the LED solid on hardware: a game
/// that saved and then ran from RAM left it blue forever. Brightness has to come
/// from duty cycle alone.
void ledPrepareForSleep(void);

/// @brief Latches red on and clears blue, to report a fault the firmware cannot
///        recover from. Idempotent.
///
/// Deliberately out of line. It is called from six __scratch_y handlers, and as
/// a static inline the store was duplicated into each of them, growing the
/// RAM resident time critical section for no benefit: this path is terminal, so
/// the cost of a call into flash does not matter here.
void ledSignalError(void);
