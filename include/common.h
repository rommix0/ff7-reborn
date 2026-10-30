#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"

/* The body of a loop that waits on something only an interrupt changes. The PC build's hardware moves only when
 * polled (pc/hw.c), so there the wait gives it a turn; on the PS1 it's empty. */
#ifdef NATIVE
void hw_poll(void);
#define HW_WAIT() hw_poll()
#else
#define HW_WAIT()
#endif

#endif
