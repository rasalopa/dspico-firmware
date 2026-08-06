#pragma once
#include <stdint.h>

#ifdef ENABLE_STATUS_LEDS

// Bumped from SdCard::StateReadBegin and StateWriteBegin, which every transfer
// passes through, the r4 and FatFs ones included. Counting there rather than in
// the TryBegin* header inlines keeps this out of the __scratch_y card handlers,
// which core 0's stack grows down into. A lost increment costs at most one
// missed blink, so the plain ++ is enough.
extern volatile uint32_t gLedSdReadEvents;
extern volatile uint32_t gLedSdWriteEvents;

/// @brief Records that the card state machine has taken up a read.
static inline void ledNotifySdRead(void) { gLedSdReadEvents++; }

/// @brief Records that the card state machine has taken up a write.
static inline void ledNotifySdWrite(void) { gLedSdWriteEvents++; }

/// @brief Claims the two LED pins and parks both dark.
void ledInit(void);

/// @brief Drives the blue LED from the counters above. Call once per main loop pass.
void ledUpdate(void);

/// @brief Clears the blue LED before the main loop sleeps. Has to run every pass,
///        since the loop only wakes on an interrupt and a level left set stays set.
void ledPrepareForSleep(void);

/// @brief Latches red on and clears blue, to report an unrecoverable fault.
///
/// Out of line because it is called from __scratch_y handlers, where a static
/// inline duplicated the store into each of them and grew the RAM resident time
/// critical section.
void ledSignalError(void);

#else

// With the flag off the call sites stay as they are and compile to nothing.
static inline void ledNotifySdRead(void) { }
static inline void ledNotifySdWrite(void) { }
static inline void ledInit(void) { }
static inline void ledUpdate(void) { }
static inline void ledPrepareForSleep(void) { }
static inline void ledSignalError(void) { }

#endif
