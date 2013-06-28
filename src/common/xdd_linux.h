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
 *       Steve Hodson, DoE/ORNL, (hodsonsw@ornl.gov)
 *       Steve Poole, DoE/ORNL, (spoole@ornl.gov)
 *       Brad Settlemyer, DoE/ORNL (settlemyerbw@ornl.gov)
 *       Russell Cattelan, Digital Elves (russell@thebarn.com)
 *       Alex Elder
 * Funding and resources provided by:
 * Oak Ridge National Labs, Department of Energy and Department of Defense
 *  Extreme Scale Systems Center ( ESSC ) http://www.csm.ornl.gov/essc/
 *  and the wonderful people at I/O Performance, Inc.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <libgen.h>
#include <unistd.h> 
#include <ctype.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/times.h>
#include <sys/prctl.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/resource.h> /* needed for multiple processes */
#include <pthread.h>
#include <sched.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <string.h>
#include <syscall.h>
/* for the global clock stuff */
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#if (SNFS)
#include <client/cvdrfile.h>
#endif
#include "xint_nclk.h" /* nclk_t, prototype compatibility */
#include "xint_misc.h"

#define MP_MUSTRUN 1 /* Assign this thread to a specific processor */
#define MP_NPROCS 2 /* return the number of processors on the system */
typedef int  sd_t;  /* A socket descriptor */
#define CLK_TCK sysconf(_SC_CLK_TCK)
#define DFL_FL_ADDR INADDR_ANY /* Any address */  /* server only */
#define closesocket(sd) close(sd)

#include "xint_restart.h"

extern int h_errno; // For socket calls

#if XFS_ENABLED && HAVE_XFS_XFS_H
#include <xfs/xfs.h>
#elif XFS_ENABLED && HAVE_XFS_LIBXFS_H
#include <xfs/xfs.h>
#include <xfs/libxfs.h>
#elif XFS_ENABLED
#error "ERROR: XFS Support is enabled, but the header support is not valid."
#endif

#if HAVE_LINUX_MAGIC_H && HAVE_DECL_XFS_SUPER_MAGIC
#include <linux/magic.h>
#else
#define XFS_SUPER_MAGIC 0x58465342
#endif

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
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