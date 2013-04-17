/* Copyright (C) 1992-2010 I/O Performance, Inc. and the
 * United States Departments of Energy (DoE) and Defense (DoD)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named 'Copying'; if not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139.
 */
/* Principal Author:
 *      Tom Ruwart (tmruwart@ioperformance.com)
 * Contributing Authors:
 *       Steve Hodson, DoE/ORNL
 *       Steve Poole, DoE/ORNL
 *       Brad Settlemyer, DoE/ORNL
 *       Russell Cattelan, Digital Elves
 *       Alex Elder
 * Funding and resources provided by:
 * Oak Ridge National Labs, Department of Energy and Department of Defense
 *  Extreme Scale Systems Center ( ESSC ) http://www.csm.ornl.gov/essc/
 *  and the wonderful people at I/O Performance, Inc.
 */
/*
 * This set of routines is used in accessing a system clock.
 */
/* -------- */
/* Includes */
/* -------- */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include "xint_nclk.h" /* nclk_t, prototype compatibility */

/* --------------- */
/* Private globals */
/* --------------- */
/* Nothing works until the nclk subsystem is initialized. */
static bool     nclk_initialized = false;
/*----------------------------------------------------------------------------*/
/* nclk_initialize()
 *
 * Initialize the clock used to record time stamps for timers.  Returns
 * the resolution of the clock (nanoseconds per tick)
 * In some operating systems there is an actual "initialization" routine that
 * needs to be called but most of those have gone away so the initialization 
 * function is simply returning the resolution of the clock to the caller. 
 *
 */
void
nclk_initialize(nclk_t *nclkp) {

#if (LINUX)
	// Since we use the "nanosecond" clocks in Linux, 
	// the number of nanoseconds per nanosecond "tick" is 1
    *nclkp =  ONE;
#elif (SOLARIS || AIX || DARWIN || FREEBSD )
	// Since we use the "gettimeofday" clocks in this OS, 
	// the number of nanoseconds per microsecond "tick" is 1,000
    *nclkp =  THOUSAND;
#endif
    nclk_initialized = true;
    return;
}
/*----------------------------------------------------------------------------*/
/*
 * nclk_shutdown()
 *
 * De-initialize the clock.
 */
void
nclk_shutdown(void) {
    if (nclk_initialized) 
		nclk_initialized = false;
}

/*----------------------------------------------------------------------------*/
/*
 *  * nclk_now()
 *   *
 *    * Return the current value of the high resolution clock, in nanoseconds.
 *     * This timer is designed to store results in a 64-bit integer wrt Epoch
 *      * for direct comparison to timestamps from tools other than xdd, such
 *       * as kernel tracing tools.
 *        * Note that Epoch seconds x 1e9 + nanoseconds will consume 18 decimal digits
 *         * This should fit into unsigned 64-bit integer.
 *          */
#ifdef WIN32
void
nclk_now(nclk_t *nclkp) {

		QueryPerformanceCounter((LARGE_INTEGER *)nclkp);
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#elif (LINUX)
void
nclk_now(nclk_t *nclkp) {

#ifdef _POSIX_TIMERS
        struct timespec current_time;
        clock_gettime(CLOCK_REALTIME, &current_time);
        *nclkp =  (nclk_t)(((nclk_t)current_time.tv_sec * BILLION) +
                           ((nclk_t)current_time.tv_nsec ));
#else
        struct timeval current_time;
        struct timezone tz;
        gettimeofday(&current_time, &tz);
        *nclkp =  (nclk_t)(((nclk_t)current_time.tv_sec  * BILLION) +
                           ((nclk_t)current_time.tv_usec * THOUSAND));
#endif
    return;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#elif (SOLARIS || AIX || DARWIN || FREEBSD )
void
nclk_now(nclk_t *nclkp) {
    struct timeval current_time;
    struct timezone tz;

        gettimeofday(&current_time, &tz);
    	*nclkp =  (nclk_t)(((nclk_t)current_time.tv_sec  * BILLION) +
                           ((nclk_t)current_time.tv_usec * THOUSAND));
    return;
}
#endif


/*
 * Local variables:
 *  indent-tabs-mode: t
 *  default-tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=4 sts=4 sw=4 noexpandtab
 */
