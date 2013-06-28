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
#include "xint.h"

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

/*----------------------------------------------------------------------------*/
/* xdd_worker_thread_init() - Initialization routine for a WorkerThread
 * This routine is passed a pointer to the Data Struct for this WorkerThread.
 * Return values: 0 is good, -1 is bad
 */
int32_t
xdd_worker_thread_init(worker_data_t *wdp) {
    int32_t  	status;
    target_data_t		*tdp;			// Pointer to this worker_thread's target Data Struct
    char		tmpname[XDD_BARRIER_MAX_NAME_LENGTH];	// Used to create unique names for the barriers

#if defined(HAVE_CPUSET_T) && defined(HAVE_PTHREAD_ATTR_SETAFFINITY_NP)
    // BWS Print the cpuset
    int i;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    printf("Thread %d bound to NUMA node", pthread_self());
    for (i = 0; i< 48; i++)
        if (CPU_ISSET(i, &cpuset))
            printf(" %d", i);
    printf("\n");
#endif

    // Get the Target Data Struct address as well
    tdp = wdp->wd_tdp;

#if (AIX)
	wdp->wd_thread_id = thread_self();
#elif (LINUX)
	wdp->wd_thread_id = syscall(SYS_gettid);
#else
	wdp->wd_thread_id = wdp->wd_pid;
#endif

	// The "my_current_state_mutex" is used by the WorkerThreads when checking or updating the state info
	status = pthread_mutex_init(&tdp->td_current_state_mutex, 0);
	if (status) {
		fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: ERROR: Cannot init my_current_state_mutex \n",
			xgp->progname, 
			tdp->td_target_number,
			wdp->wd_thread_number);
		fflush(xgp->errout);
		return(-1);
	}
	// The "mutex_this_worker_thread_is_available" is used by the WorkerThreads and the get_next_available_worker_thread() subroutines
	status = pthread_mutex_init(&tdp->td_worker_thread_target_sync_mutex, 0);
	if (status) {
		fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: ERROR: Cannot init worker_thread_target_sync_mutex\n",
			xgp->progname, 
			tdp->td_target_number,
			wdp->wd_thread_number);
		fflush(xgp->errout);
		return(-1);
	}
	tdp->td_worker_thread_target_sync = 0;

#if (LINUX || AIX)
        // Copy the file descriptor from the target thread (requires pread/pwrite support)
        status = xdd_target_shallow_open(wdp);
#else
	// Open the target device/file
	status = xdd_target_open(wdp);
#endif
	if (status < 0) {
		fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: ERROR: Failed to open Target named '%s'\n",
			xgp->progname,
			tdp->td_target_number,
			wdp->wd_thread_number,
			tdp->td_target_full_pathname);
		return(-1);
	}
        
	// Get a RW buffer
	wdp->wd_task.task_bufp = xdd_init_io_buffers(wdp);
	if (wdp->wd_task.task_bufp == 0) {
		fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: ERROR: Failed to allocate I/O buffer.\n",
			xgp->progname,
			tdp->td_target_number,
			wdp->wd_thread_number);
		return(-1);
	}

	// Set proper data pattern in RW buffer
	xdd_datapattern_buffer_init(wdp);

	// Init the WorkerThread-TargetPass WAIT Barrier for this WorkerThread
	sprintf(tmpname,"T%04d:W%04d>worker_thread_targetpass_wait_barrier",tdp->td_target_number,wdp->wd_thread_number);
	status = xdd_init_barrier(tdp->td_planp, &wdp->wd_thread_targetpass_wait_for_task_barrier, 2, tmpname);
	if (status) {
		fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: ERROR: Cannot initialize WorkerThread TargetPass WAIT barrier.\n",
			xgp->progname, 
			tdp->td_target_number,
			wdp->wd_thread_number);
		fflush(xgp->errout);
		return(-1);
	}

	// Initialize the worker_thread ordering 
	status = pthread_cond_init(&wdp->wd_this_worker_thread_is_available_condition, 0);

	// Indicate to the Target Thread that this WorkerThread is available
	pthread_mutex_lock(&tdp->td_any_worker_thread_available_mutex);
	tdp->td_any_worker_thread_available++;
	status = pthread_cond_broadcast(&tdp->td_any_worker_thread_available_condition);
	pthread_mutex_unlock(&tdp->td_any_worker_thread_available_mutex);
	if (status) {
		fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: WARNING: Bad status from sem_post on any_worker_thread_available semaphore: status=%d, errno=%d\n",
			xgp->progname,
			tdp->td_target_number,
			wdp->wd_thread_number,
			status,
			errno);
	}

	// Set up for an End-to-End operation (if requested)
	if (tdp->td_target_options & TO_ENDTOEND) {
		if (tdp->td_target_options & (TO_E2E_DESTINATION|TO_E2E_SOURCE)) {
			status = xdd_e2e_worker_init(wdp);
		} else { // Not sure which side of the E2E this target is supposed to be....
			fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: Cannot determine which side of the E2E operation this target is supposed to be.\n",
				xgp->progname,
				tdp->td_target_number,
				wdp->wd_thread_number);
			fprintf(xgp->errout,"%s: xdd_worker_thread_init: Check to be sure that the '-e2e issource' or '-e2e isdestination' was specified for this target.\n",
				xgp->progname);
				fflush(xgp->errout);
			return(-1);
		}

		if (status == -1) {
			fprintf(xgp->errout,"%s: xdd_worker_thread_init: Target %d WorkerThread %d: E2E %s initialization failed.\n",
				xgp->progname,
				tdp->td_target_number,
				wdp->wd_thread_number,
				(tdp->td_target_options & TO_E2E_DESTINATION) ? "DESTINATION":"SOURCE");
		return(-1);
		}
	} // End of end-to-end setup

	// All went well...
	return(0);

} // End of xdd_worker_thread_init()
 
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