#include "schedos-app.h"
#include "x86sync.h"

/*****************************************************************************
 * schedos-1
 *
 *   This tiny application prints red "1"s to the console.
 *   It yields the CPU to the kernel after each "1" using the sys_yield()
 *   system call.  This lets the kernel (schedos-kern.c) pick another
 *   application to run, if it wants.
 *
 *   The other schedos-* processes simply #include this file after defining
 *   PRINTCHAR appropriately.
 *
 *****************************************************************************/

#ifndef PRINTCHAR
#define PRINTCHAR	('1' | 0x0C00)
#endif

#ifndef PRIORITY
#define PRIORITY	4
#endif

#ifndef SHARE
#define SHARE		4
#endif

// Preprocessor symbol for changing between 2 synchronization mechanisms
#define CHANGE_SYNC

// UNCOMMENT THE NEXT LINE TO USE EXERCISE 8 CODE INSTEAD OF EXERCISE 6
// #define __EXERCISE_8__
// Use the following structure to choose between them:
// #infdef __EXERCISE_8__
// (exercise 6 code)
// #else
// (exercise 8 code)
// #endif


void
start(void)
{
	sys_priority(PRIORITY);
	sys_share(SHARE);

	int i;
	for (i = 0; i < RUNCOUNT; i++) {
		// Write characters to the console, yielding after each one.
		//*cursorpos++ = PRINTCHAR;
		//sys_yield();
		#ifndef CHANGE_SYNC
			sys_print(PRINTCHAR);
		#endif
		
		#ifdef CHANGE_SYNC
			// spin_lock's address is specified in schedos-symbols.ld
			while(atomic_swap(&spin_lock, 1) != 0)	// attempt to acquire the lock
				continue;
			*cursorpos++ = PRINTCHAR;	// write to the console
			atomic_swap(&spin_lock, 0);	// release the lock after writing is done
		#endif
		sys_yield();
	}

	// while (1)
		sys_exit(0);
}
