/*  linux_port.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V and John Melton, G0ORX/N6LYT

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com
john.d.melton@googlemail.com

*/

// no-port-check: Existing vendored TAPR WDSP POSIX shim, registered in
// docs/attribution/WDSP-PROVENANCE.md and checked by the WDSP header verifier.
// The local semaphore namespace repair below introduces no new Thetis port.
// NereusSDR modification history:
//   2026-09-21 — Make macOS semaphore names process-unique and unlink them
//                immediately after creation so concurrent NereusSDR/Qt test
//                processes cannot collide in the POSIX semaphore namespace.
//                J.J. Boyd (KG4VCF), with AI assistance from OpenAI Codex.
//   2026-09-23 - Keep this file's EnterCriticalSection definition the real
//                platform call: comm.h now redirects WDSP lock entry to
//                dsplock.c's WdspEnterCS, so the redirect is undefined here.
//                J.J. Boyd (KG4VCF), with AI assistance from Anthropic
//                Claude Code.
//   2026-09-23 - wdsp_beginthread starts each thread through a small
//                trampoline that names it on itself ("WDSP rx0",
//                "WDSP tx5", "WDSP flush0"; plain "WDSP" otherwise), gives a
//                channel worker user-interactive QoS on macOS, and calls the
//                application's thread-start hook (WDSPSetThreadStartHook)
//                for channel workers and flush threads before the start
//                routine runs (R-R3-41). Thread start only; no DSP change.
//                J.J. Boyd (KG4VCF), with AI assistance from Anthropic
//                Claude Code.
//   2026-09-23 - A channel worker also calls the hook with
//                WDSP_THREAD_WORKER_EXIT once its start routine returns,
//                so the application forgets a finished worker before its
//                thread ID can be reused (R-R3-41). No DSP change.
//                J.J. Boyd (KG4VCF), with AI assistance from Anthropic
//                Claude Code.

#include "linux_port.h"
#include "comm.h"

// comm.h's redirect must not rename the platform call defined below.
#undef EnterCriticalSection

#include <errno.h>
#include <time.h>
#ifdef __APPLE__
#include <pthread/qos.h>
#endif

/********************************************************************************************************
*													*
*	Linux Port Utilities										*
*													*
********************************************************************************************************/

#if defined(linux) || defined(__APPLE__)

void QueueUserWorkItem(void *function,void *context,int flags) {
	pthread_t t;
	pthread_create(&t, NULL, function, context);
	pthread_join(t, NULL);
}

void InitializeCriticalSectionAndSpinCount(pthread_mutex_t *mutex,int count) {
	pthread_mutexattr_t mAttr;
	pthread_mutexattr_init(&mAttr);
#ifdef __APPLE__
	// DL1YCF: MacOS X does not have PTHREAD_MUTEX_RECURSIVE_NP
	pthread_mutexattr_settype(&mAttr,PTHREAD_MUTEX_RECURSIVE);
#else
	pthread_mutexattr_settype(&mAttr,PTHREAD_MUTEX_RECURSIVE_NP);
#endif
	pthread_mutex_init(mutex,&mAttr);
	pthread_mutexattr_destroy(&mAttr);
	// ignore count
}

void EnterCriticalSection(pthread_mutex_t *mutex) {
	pthread_mutex_lock(mutex);
}

void LeaveCriticalSection(pthread_mutex_t *mutex) {
	pthread_mutex_unlock(mutex);
}

void DeleteCriticalSection(pthread_mutex_t *mutex) {
	pthread_mutex_destroy(mutex);
}

static uint64_t monotonic_milliseconds(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

int LinuxWaitForSingleObject(sem_t *sem,int ms) {
	uint64_t deadline;
	if (sem == 0) return (int)WAIT_FAILED;
	if (ms == INFINITE) {
		while (sem_wait(sem) != 0) {
			if (errno != EINTR) return (int)WAIT_FAILED;
		}
		return (int)WAIT_OBJECT_0;
	}
	if (ms < 0) return (int)WAIT_FAILED;
	deadline = monotonic_milliseconds() + (uint64_t)ms;
	for (;;) {
		if (sem_trywait(sem) == 0) return (int)WAIT_OBJECT_0;
		if (errno != EAGAIN && errno != EINTR) return (int)WAIT_FAILED;
		if (monotonic_milliseconds() >= deadline) return (int)WAIT_TIMEOUT;
		{
			const struct timespec pause = { 0, 1000000 };
			nanosleep(&pause, 0);
		}
	}
}

unsigned int LinuxWaitForMultipleObjects(unsigned int count, HANDLE* handles,
	int wait_all, int milliseconds)
{
	static unsigned int next_start;
	uint64_t deadline;
	unsigned int start;
	if (wait_all) {
		errno = ENOTSUP;
		return WAIT_FAILED;
	}
	if (count == 0 || handles == 0 || milliseconds < INFINITE) {
		errno = EINVAL;
		return WAIT_FAILED;
	}
	start = __sync_fetch_and_add(&next_start, 1u) % count;
	deadline = milliseconds == INFINITE ? 0u
		: monotonic_milliseconds() + (uint64_t)milliseconds;
	for (;;) {
		unsigned int offset;
		for (offset = 0; offset < count; ++offset) {
			const unsigned int index = (start + offset) % count;
			sem_t* sem = (sem_t*)handles[index];
			if (sem == 0) {
				errno = EINVAL;
				return WAIT_FAILED;
			}
			if (sem_trywait(sem) == 0) return WAIT_OBJECT_0 + index;
			if (errno != EAGAIN && errno != EINTR) return WAIT_FAILED;
		}
		if (milliseconds != INFINITE && monotonic_milliseconds() >= deadline)
			return WAIT_TIMEOUT;
		{
			const struct timespec pause = { 0, 1000000 };
			nanosleep(&pause, 0);
		}
		start = (start + 1u) % count;
	}
}

sem_t *LinuxCreateSemaphore(int attributes,int initial_count,int maximum_count,char *name) {
        sem_t *sem;
#ifdef __APPLE__
	//DL1YCF
	//This routine is invoked with name=NULL several times, so we have to make
	//a unique name of tpye WDSPxxxxx for each invocation.
	// NereusSDR: macOS only provides named process-shared semaphores.  The
	// upstream process-local WDSPxxxxx counter collides when independent
	// NereusSDR processes create WDSP channels concurrently: both can unlink
	// the name before either sem_open(O_EXCL), leaving one with SEM_FAILED.
	// Include the PID, serialize this process's counter, and unlink a
	// successful semaphore immediately.  POSIX keeps the object alive until
	// CloseHandle's sem_close while removing its cross-process namespace.
	static pthread_mutex_t semname_mutex = PTHREAD_MUTEX_INITIALIZER;
	static unsigned int semcount = 0;
	char sname[32];
	int attempts;

	sem = SEM_FAILED;
	pthread_mutex_lock(&semname_mutex);
	for (attempts = 0; attempts < 16; ++attempts)
	{
		const unsigned int sequence = semcount++;
		const int length = snprintf(sname, sizeof(sname), "/wdsp-%ld-%u",
			(long)getpid(), sequence);
		if (length < 0 || (size_t)length >= sizeof(sname))
		{
			errno = ENAMETOOLONG;
			break;
		}
		sem = sem_open(sname, O_CREAT | O_EXCL, 0700, initial_count);
		if (sem != SEM_FAILED)
		{
			sem_unlink(sname);
			break;
		}
		if (errno != EEXIST)
			break;
	}
	pthread_mutex_unlock(&semname_mutex);
	if (sem == SEM_FAILED) {
	  perror("WDSP:CreateSemaphore");
	}
#else
        sem=malloc(sizeof(sem_t));
	int result;
	// NereusSDR fix: upstream WDSP linux_port hard-coded the initial
	// count to 0, ignoring the caller's initial_count parameter. That
	// silently broke any call site that depends on a pre-signalled
	// semaphore — most importantly Sem_OutReady in iobuffs.c:427,
	// which fexchange2(bfo=1) expects to start with n free slots so
	// the first n calls don't block. On Linux this caused a
	// deterministic deadlock on the very first fexchange2 call: bfo
	// wait saw count=0, blocked; wdspmain was waiting for
	// Sem_BuffReady which only releases after dsp_insize/in_size
	// fexchange2 calls — circular wait with no escape. The macOS
	// sem_open path (above) already uses initial_count correctly.
	result=sem_init(sem, 0, initial_count);
        if (result < 0) {
	  perror("WDSP:CreateSemaphore");
        }
#endif
	return sem;
}

void LinuxReleaseSemaphore(sem_t* sem,int release_count, int* previous_count) {
	while(release_count>0) {
		sem_post(sem);
		release_count--;
	}
}

sem_t *CreateEvent(void* security_attributes,int bManualReset,int bInitialState,char* name) {
	int result;
        sem_t *sem;
	sem=LinuxCreateSemaphore(0,bInitialState ? 1 : 0,1,0);
	// need to handle bManualReset and bInitialState
	return sem;
}

void LinuxSetEvent(sem_t* sem) {
	sem_post(sem);
}

void LinuxResetEvent(sem_t* sem) {
	// Drain the semaphore (non-blocking) to reset it to zero
	while (sem_trywait(sem) == 0) { }
}

// NereusSDR: the application's thread-start hook (0 = none). Written by
// WDSPSetThreadStartHook, read once by each new thread.
static void (*wdsp_thread_start_hook)(int kind, int channel) = 0;

PORT void WDSPSetThreadStartHook (void (*hook)(int kind, int channel))
{
	__atomic_store_n(&wdsp_thread_start_hook, hook, __ATOMIC_RELEASE);
}

// NereusSDR: what a new thread needs to run its start routine.
struct wdsp_thread_start {
	void (*start_address)(void *);
	void *arglist;
};

// NereusSDR: runs first on every thread wdsp_beginthread starts. It names
// the thread after its job, raises a channel worker's QoS on macOS and
// reports channel workers and flush threads to the application's hook, then
// runs the start routine exactly as before.
static void *wdsp_thread_trampoline(void *p)
{
	struct wdsp_thread_start start = *(struct wdsp_thread_start *)p;
	int kind = 0;
	int channel = -1;
	char name[16];
	void (*hook)(int, int);

	free(p);
	if (start.start_address == wdspmain) {
		channel = (int)(uintptr_t)start.arglist;
		kind = ch[channel].type == 1 ? WDSP_THREAD_TX_MAIN : WDSP_THREAD_RX_MAIN;
		snprintf(name, sizeof(name), "WDSP %s%d",
			kind == WDSP_THREAD_TX_MAIN ? "tx" : "rx", channel);
	} else if (start.start_address == flushChannel) {
		channel = (int)(uintptr_t)start.arglist;
		kind = WDSP_THREAD_FLUSH;
		snprintf(name, sizeof(name), "WDSP flush%d", channel);
	} else {
		snprintf(name, sizeof(name), "WDSP");
	}
#ifdef __APPLE__
	pthread_setname_np(name);
	if (kind == WDSP_THREAD_RX_MAIN || kind == WDSP_THREAD_TX_MAIN)
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#else
	pthread_setname_np(pthread_self(), name);
#endif
	hook = __atomic_load_n(&wdsp_thread_start_hook, __ATOMIC_ACQUIRE);
	if (hook != 0 && kind != 0)
		hook(kind, channel);
	start.start_address(start.arglist);
	// NereusSDR: a worker is gone once wdspmain returns; say so while this
	// thread's ID is still its own.
	if (kind == WDSP_THREAD_RX_MAIN || kind == WDSP_THREAD_TX_MAIN)
	{
		hook = __atomic_load_n(&wdsp_thread_start_hook, __ATOMIC_ACQUIRE);
		if (hook != 0)
			hook(WDSP_THREAD_WORKER_EXIT, channel);
	}
	return 0;
}

HANDLE wdsp_beginthread( void( __cdecl *start_address )( void * ), unsigned stack_size, void *arglist) {
	pthread_t threadid;
	pthread_attr_t  attr;
	int rc = 0;
	struct wdsp_thread_start *start;

	if (rc = pthread_attr_init(&attr)) {
 	    return (HANDLE)-1;
	}
      
	if(stack_size!=0) {
	    if (rc = pthread_attr_setstacksize(&attr, stack_size)) {
	        return (HANDLE)-1;
	    }
	}

        if( rc = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)) {
            return (HANDLE)-1;
        }

	// NereusSDR: the new thread names and reports itself in
	// wdsp_thread_trampoline, which frees this.
	start = malloc(sizeof(*start));
	if (start == 0) {
	    return (HANDLE)-1;
	}
	start->start_address = start_address;
	start->arglist = arglist;
     
	if (rc = pthread_create(&threadid, &attr, wdsp_thread_trampoline, start)) {
	     free(start);
	     return (HANDLE)-1;
	}

        //pthread_attr_destroy(&attr);
	// DL1YCF: this function does not exist on MacOS. You can only name the
        //         current thread.
	// NereusSDR: every platform now names the thread on itself, in
	// wdsp_thread_trampoline, so the name reflects the thread's job.

	return (HANDLE)threadid;

}

void _endthread() {
	int res;
	pthread_exit((void *)&res);
}

void SetThreadPriority(HANDLE thread, int priority)  {
/*
	int policy;
	struct sched_param param;

	pthread_getschedparam(thread, &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);
	pthread_setschedparam(thread, policy, &param);
*/
}

int CloseHandle(HANDLE hObject) {
//
// This routine is *ONLY* called to release semaphores
//
#ifdef __APPLE__
//
// A semaphore is closed and re-allocated on each RX->TX transition.
// After about 200 RX/TX transitions, MacOS runs out of file descriptors
// since MacOS only has named semaphores. As a consequence,
// no new semaphores can be allocated, and other parts of the program cannot
// open new files ore make new connections.
// Therefore we should close the semaphore.
//
if (sem_close((sem_t *)hObject) < 0) {
  perror("WDSP:CloseHandle:SemCLose");
}
#else
//
// Although the number of semaphores seems "unlimited" on RapianOS,
// this is nevertheless a memory leak (a sem_t is allocated before
// sem_init is called, see above).
// So destroy the semaphore and (if this was successful) release the memory.
//

if (sem_destroy((sem_t *)hObject) < 0) {
  perror("WDSP:CloseHandle:SemDestroy");
} else {
  // if sem_destroy failed, do not release storage
  free(hObject);
}
#endif

// this is actually a void function (return value never used).
return 0;
}

#endif
