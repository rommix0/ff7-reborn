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

/* A call through a code address in the game's data. On the PC the exe's own are native pointers once patched, but a
 * module's is still its game address, so the call goes through the dispatcher. */
#ifdef NATIVE
int data_call(unsigned int addr, int a0);
#define DATA_CALL(f, a0) data_call((unsigned int)(f), a0)
#define DATA_CALL0(f) data_call((unsigned int)(f), 0)
#else
#define DATA_CALL(f, a0) (f)(a0)
#define DATA_CALL0(f) (f)()
#endif

#endif
