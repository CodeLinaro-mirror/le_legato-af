//--------------------------------------------------------------------------------------------------
/** @file signals.c
 *
 * This file implements the Legato Signal Events by making use of signalFd.  When the user sets a
 * signal event handler the handler is stored in a list of handlers and associated with a single
 * signal number.  The signal mask for the thread is then updated.
 *
 * Each thread has its own list of handlers and stores this list in the thread's local data.
 *
 * A monitor fd is created for each thread with atleast one handler but all monitor fds share a
 * single fd handler, OurSigHandler().  When OurSigHandler() is invoked it grabs the list of
 * handlers for the current thread and routes the signal to the proper user handler.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "limit.h"
#include "signals.h"
#include "backtrace.h"
#include "logPlatform.h"

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include <ucontext.h>
#include <syslog.h>

//--------------------------------------------------------------------------------------------------
/**
 * The signal event monitor object.  There should be at most one of these per thread.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_fdMonitor_Ref_t  monitorRef;
    int                 fd;
    le_dls_List_t       handlerObjList;
}
MonitorObj_t;


//--------------------------------------------------------------------------------------------------
/**
 * The signal event handler object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    int                         sigNum;
    le_sig_EventHandlerFunc_t   handler;
    le_dls_Link_t               link;
}
HandlerObj_t;


//--------------------------------------------------------------------------------------------------
/**
 * The signal event monitor object memory pool.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t MonitorObjPool;


//--------------------------------------------------------------------------------------------------
/**
 * The signal event handler object memory pool.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t HandlerObjPool;


//--------------------------------------------------------------------------------------------------
/**
 * The thread local data's key for monitor objects.
 */
//--------------------------------------------------------------------------------------------------
static pthread_key_t SigMonKey;


//--------------------------------------------------------------------------------------------------
/**
 * Port to use for start and attach a gdbserver(1) to itself. If 0, no gdbserver(1) is started
 */
//--------------------------------------------------------------------------------------------------
static uint32_t GdbServerPort = 0;


//--------------------------------------------------------------------------------------------------
/**
 * Flag set atomically at the very start of ShowStackSignalHandler to indicate that a crash
 * backtrace is in progress on some thread.
 *
 * Used by CrashWaitAtExit() — a framework atexit handler registered by
 * le_sig_InstallShowStackHandler() — to delay _exit() long enough for the crash handler to
 * finish writing its output before the process tears down.
 *
 * Declared volatile sig_atomic_t so that the write in the signal handler and the read in the
 * atexit handler are both atomic and visible across threads without requiring a mutex (which
 * would be unsafe inside a signal handler).
 */
//--------------------------------------------------------------------------------------------------
static volatile sig_atomic_t g_crashInProgress = 0;


//--------------------------------------------------------------------------------------------------
/**
 * Prefix for the monitor's name.  The monitor's name is this prefix plus the name of the thread.
 */
//--------------------------------------------------------------------------------------------------
#define SIG_STR     "Sig"


//--------------------------------------------------------------------------------------------------
/**
 * Returns the handler object with the matching sigNum from the list.
 *
 * @return
 *      A pointer to the handler object with a matching sigNum if found.
 *      NULL if a matching sigNum could not be found.
 */
//--------------------------------------------------------------------------------------------------
static HandlerObj_t* FindHandlerObj
(
    const int sigNum,
    le_dls_List_t* listPtr
)
{
    // Search for the sigNum from the list.
    le_dls_Link_t* handlerLinkPtr = le_dls_Peek(listPtr);

    while (handlerLinkPtr != NULL)
    {
        HandlerObj_t* handlerObjPtr = CONTAINER_OF(handlerLinkPtr, HandlerObj_t, link);

        if (handlerObjPtr->sigNum == sigNum)
        {
            return handlerObjPtr;
        }

        handlerLinkPtr = le_dls_PeekNext(listPtr, handlerLinkPtr);
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Our signal handler.  This signal handler gets called whenever any unmasked signals are received.
 * This handler will read the signal info and call the appropriate user handler.
 */
//--------------------------------------------------------------------------------------------------
static void OurSigHandler
(
    int fd,         ///< The monitored file descriptor.
    short events    ///< The event or events (bit mask) that occurred on the fd.
)
{
    if (events & ~POLLIN)
    {
        LE_CRIT("Unexpected event set (0x%hx) from signal fd.", events);
        if ((events & POLLIN) == 0)
        {
            return;
        }
    }

    while(1)
    {
        // Do a read of the signal fd.
        struct signalfd_siginfo sigInfo;
        int numBytesRead = read(fd, &sigInfo, sizeof(sigInfo));

        if (numBytesRead  > 0)
        {
            // Get our thread's monitor object.
            MonitorObj_t* monitorObjPtr = pthread_getspecific(SigMonKey);
            LE_ASSERT(monitorObjPtr != NULL);

            // Find the handler object with the same signal as the one we just received.
            HandlerObj_t* handlerObjPtr = FindHandlerObj(sigInfo.ssi_signo, &(monitorObjPtr->handlerObjList));

            // Call the handler function.
            if ( (handlerObjPtr != NULL) && (handlerObjPtr->handler != NULL) )
            {
                handlerObjPtr->handler(sigInfo.ssi_signo);
            }
        }
        else if ( (numBytesRead == 0) || ((numBytesRead == -1) && (errno == EAGAIN)) )
        {
            // Nothing more to read.
            break;
        }
        else if ( (numBytesRead == -1) && (errno != EINTR) )
        {
            LE_FATAL("Could not read from signal fd: %m");
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Our show stack signal handler. This signal handler is called only when SEGV, ILL, BUS, FPE, ABRT
 * TRAP are raised. It will show useful informations: signal, fault address, fault PC, registers,
 * stack and back-trace. It also dumps the process maps.
 * Note: Because these signals are raised from low-level, we should avoid any usage of malloc(3),
 * syslog(3) and others services like these from stdio(3).
 *
 * @note This code is architecture dependant, and supports arm, x86_64, i586 and i686.
 * @note All formatting uses async-signal-safe helpers (SigWriteStr/SigWriteULong/SigWritePtr)
 *       instead of snprintf(). snprintf() is NOT on the POSIX async-signal-safe list because
 *       it accesses locale state and may call malloc() internally, both of which can deadlock
 *       if the signal interrupted the process while those locks were already held.
 */
//--------------------------------------------------------------------------------------------------
static void ShowStackSignalHandler
(
    int sigNum,
    siginfo_t* sigInfoPtr,
    void* sigVoidPtr
)
{
    /* sigString is used only as a raw byte buffer for read() — never for snprintf(). */
    char sigString[256];
    /* pathBuf holds /proc/<pid>/... paths built with async-signal-safe helpers. */
    char pathBuf[64];
    int  pathLen;
    int  fd;
    pid_t tid = syscall(SYS_gettid);
    void* pcPtr = NULL;

    /* Signal to CrashWaitAtExit() that a crash handler is now running.
     * This must be the very first write in this handler so that if SIGTERM
     * arrives concurrently and TermSignalHandler calls exit(), the framework
     * atexit handler will see the flag and wait for us to finish. */
    g_crashInProgress = 1;

    // Guard against a NULL context pointer before dereferencing it. The kernel should always
    // provide a valid ucontext for synchronous fault signals, but be defensive.
    if (sigVoidPtr != NULL)
    {
        struct sigcontext* ctxPtr =
            (struct sigcontext *)&(((ucontext_t*)sigVoidPtr)->uc_mcontext);
#if defined(__arm__)
    pcPtr = (void*)ctxPtr->arm_pc;
#elif defined(__i586__) || defined(__i686__)
    pcPtr = (void*)ctxPtr->eip;
#elif defined(__x86_64__)
    pcPtr = (void*)ctxPtr->rip;
#elif defined(__mips__)
    pcPtr = (void*)ctxPtr->sc_pc;
#elif defined(__aarch64__)
    pcPtr = (void*)ctxPtr->pc;
#else
#       warning "Architecture is not supported"
#endif
    }

    // Show process, pid and tid
    // All output uses SigWriteStr/SigWriteULong/SigWritePtr — async-signal-safe.
    // snprintf() is intentionally avoided: it is NOT async-signal-safe (accesses locale
    // state and may call malloc(), both of which can deadlock if the signal interrupted
    // the process while those locks were already held).
    SigWriteStr("PROCESS: ");
    SigWriteULong((unsigned long)getpid());
    SigWriteStr(" ,TID ");
    SigWriteULong((unsigned long)tid);
    SigWriteStr("\n");

    // Show signal, fault address and fault PC
    SigWriteStr("SIGNAL: ");
    SigWriteULong((unsigned long)sigNum);
    SigWriteStr(", ADDR ");
    SigWriteHexPtr((SIGABRT == sigNum) ? NULL : sigInfoPtr->si_addr);
    SigWriteStr(", AT ");
    SigWriteHexPtr(pcPtr);
    SigWriteStr(" SI_CODE ");
    SigWriteHexPtr((void*)(uintptr_t)(unsigned int)sigInfoPtr->si_code); /* hex fits nicely */
    SigWriteStr("\n");

    // Explain signal
    switch( sigNum )
    {
        case SIGSEGV:
                    SigWriteStr("ILLEGAL ADDRESS ");
                    SigWriteHexPtr(sigInfoPtr->si_addr);
                    SigWriteStr("\n");
                    break;
        case SIGFPE:
                    SigWriteStr("FLOATING POINT EXCEPTION AT ");
                    SigWriteHexPtr(sigInfoPtr->si_addr);
                    SigWriteStr("\n");
                    break;
        case SIGTRAP:
                    SigWriteStr("TRAP AT ");
                    SigWriteHexPtr(sigInfoPtr->si_addr);
                    SigWriteStr("\n");
                    break;
        case SIGABRT:
                    SigWriteStr("ABORT\n");
                    break;
        case SIGILL:
                    SigWriteStr("ILLEGAL INSTRUCTION AT ");
                    SigWriteHexPtr(sigInfoPtr->si_addr);
                    SigWriteStr("\n");
                    break;
        case SIGBUS:
                    SigWriteStr("BUS ERROR AT ");
                    SigWriteHexPtr(sigInfoPtr->si_addr);
                    SigWriteStr("\n");
                    break;
        default:
                    SigWriteStr("UNEXPECTED SIGNAL ");
                    SigWriteULong((unsigned long)sigNum);
                    SigWriteStr("\n");
                    break;
    }

    // Dump the legato version
    SigWriteStr("TELAF VERSION\n");
    fd = open("/legato/systems/current/version", O_RDONLY);
    if (-1 != fd)
    {
        int rc;
        // We cannot use stdio(3) services. Print line by line
        rc = read( fd, sigString, sizeof(sigString) );
        close(fd);
        if (0 < rc)
        {
            SIG_WRITE(sigString, rc);
        }
        SIG_WRITE("\n", 1);
    }

    // Dump some process command line
    SigWriteStr("PROCESS COMMAND LINE\n");
    // Build the /proc path using async-signal-safe helpers into pathBuf.
    // snprintf() is not used here for the same reason as above.
    {
        const char prefix[] = "/proc/";
        const char suffix[] = "/cmdline";
        char pidStr[21];
        int  pi = sizeof(pidStr) - 1;
        unsigned long pidVal = (unsigned long)getpid();
        pidStr[pi] = '\0';
        if (pidVal == 0) { pidStr[--pi] = '0'; }
        else { while (pidVal > 0) { pidStr[--pi] = (char)('0' + pidVal % 10); pidVal /= 10; } }
        pathLen = 0;
        memcpy(pathBuf, prefix, sizeof(prefix) - 1);  pathLen += sizeof(prefix) - 1;
        memcpy(pathBuf + pathLen, pidStr + pi, sizeof(pidStr) - 1 - (size_t)pi);
        pathLen += (int)(sizeof(pidStr) - 1 - (size_t)pi);
        memcpy(pathBuf + pathLen, suffix, sizeof(suffix));  /* includes NUL */
    }
    fd = open(pathBuf, O_RDONLY);
    if (-1 != fd)
    {
        int rc, len;
        // We cannot use stdio(3) services. Print line by line
        do
        {
            rc = read( fd, sigString, sizeof(sigString) );
            // In /proc/<pid>/cmdline, replace '\0' by ' ' in the string
            for (len = 0; len < rc; len++)
            {
                if ('\0' == sigString[len])
                {
                    sigString[len] = ' ';
                }
            }
            if (0 < rc)
            {
                SIG_WRITE(sigString, rc);
            }
        }
        while( 0 < rc );
        close(fd);
        SIG_WRITE("\n", 1);
    }

    // The process map is parsed internally by backtrace_DumpContextStack() for
    // symbol resolution. Dumping it here in full to stderr is redundant and
    // dangerous: on this target stderr is a pipe to the logger daemon, and
    // writing the entire maps file (which can be hundreds of KB with many
    // threads) can fill the pipe buffer and block write() indefinitely —
    // deadlocking the signal handler before backtrace_DumpContextStack() is
    // ever reached. Removed.

    // Dump the back-trace, registers and stack
    backtrace_DumpContextStack(sigVoidPtr, 2, sigString, sizeof(sigString), tid);

    // Check if a gdbserver(1) port is set (not zero). If yes, try to launch a
    // gdbserver(1) attached to ourself.
    if (GdbServerPort)
    {
        /* Build port string ":NNNNN" and pid string using async-signal-safe arithmetic. */
        char gdbServerPortString[8];   /* ":" + 5 digits + NUL */
        char pidString[21];
        int gdbPid, gdbStatus;
        {
            int i = sizeof(gdbServerPortString) - 1;
            unsigned int port = GdbServerPort;
            gdbServerPortString[i] = '\0';
            if (port == 0) { gdbServerPortString[--i] = '0'; }
            else { while (port > 0) { gdbServerPortString[--i] = (char)('0' + port % 10); port /= 10; } }
            gdbServerPortString[--i] = ':';
            /* shift to front of buffer so pointer arithmetic is simple */
            memmove(gdbServerPortString, gdbServerPortString + i,
                    sizeof(gdbServerPortString) - (size_t)i);
        }
        {
            int i = sizeof(pidString) - 1;
            unsigned long pv = (unsigned long)getpid();
            pidString[i] = '\0';
            if (pv == 0) { pidString[--i] = '0'; }
            else { while (pv > 0) { pidString[--i] = (char)('0' + pv % 10); pv /= 10; } }
            memmove(pidString, pidString + i, sizeof(pidString) - (size_t)i);
        }
        char *gdbArg[] =
        {
             "gdbserver",
             gdbServerPortString,
             "--attach",
             pidString,
             NULL,
        };
        if (0 == (gdbPid = fork()))
        {
            /* Use execve() with an absolute path instead of execvpe().
             * execvpe() calls getenv("PATH") internally which is NOT async-signal-safe.
             * execve() is on the POSIX async-signal-safe list. */
            execve("/usr/bin/gdbserver", gdbArg, NULL);
            /* If /usr/bin/gdbserver is not found, try the sbin location. */
            execve("/usr/sbin/gdbserver", gdbArg, NULL);
            _exit(127);
        }
        wait(&gdbStatus);
    }

    // Re-raise the signal so the kernel delivers it with the default action (core dump /
    // terminate). Because SA_RESETHAND was set, the handler disposition was already reset to
    // SIG_DFL before this handler was entered.
    //
    // Three steps are required to guarantee a core dump:
    //
    // 1. Explicitly reset the disposition to SIG_DFL (belt-and-suspenders: SA_RESETHAND should
    //    have done this, but be explicit in case another thread raced to reinstall a handler).
    //
    // 2. Unblock the signal in THIS thread's signal mask. The Legato framework blocks many
    //    signals via signalfd (le_sig_Block). If the signal is blocked, kill() / tgkill()
    //    will queue it but it will never be delivered — the process hangs instead of dumping.
    //
    // 3. Use tgkill(getpid(), tid, sigNum) to send to THIS specific thread rather than
    //    kill(getpid(), sigNum) which sends to the process and lets the kernel pick any
    //    thread. If the chosen thread has the signal blocked the delivery is deferred
    //    indefinitely. tgkill targets the crashing thread directly.
    {
        struct sigaction sa_dfl;
        sigset_t unblock;

        /* Step 1: reset disposition to SIG_DFL */
        sa_dfl.sa_handler = SIG_DFL;
        sigemptyset(&sa_dfl.sa_mask);
        sa_dfl.sa_flags = 0;
        sigaction(sigNum, &sa_dfl, NULL);

        /* Step 2: unblock the signal in this thread */
        sigemptyset(&unblock);
        sigaddset(&unblock, sigNum);
        pthread_sigmask(SIG_UNBLOCK, &unblock, NULL);

        /* Step 3: deliver to this specific thread — guarantees core dump */
        syscall(SYS_tgkill, getpid(), tid, sigNum);
    }

    // If tgkill() somehow fails to terminate the process (e.g. the signal is blocked at the
    // process level), fall back to _exit() to guarantee we do not return from the handler.
    // Returning from a handler for a synchronous fault signal (SIGSEGV, SIGBUS, SIGILL, SIGFPE)
    // is undefined behaviour and will loop forever on most kernels.
    _exit(EXIT_FAILURE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Framework atexit handler: wait for an in-progress crash backtrace to complete.
 *
 * Registered by le_sig_InstallShowStackHandler() so it runs inside exit() after all
 * application-level atexit handlers and C++ destructors have fired — which is exactly
 * the window where a worker-thread crash triggered by teardown (e.g. a NULL callback
 * invoked after a service pointer was cleared during shutdown) will occur.
 *
 * Why this works for the real scenario (e.g. tafRadioSvc):
 *
 *   exit() call order:
 *     1. App atexit handlers (registered later -> run first, LIFO)
 *        -> teardown code clears service pointers
 *        -> worker thread crashes (SIGSEGV), sets g_crashInProgress = 1
 *        -> ShowStackSignalHandler starts writing backtrace
 *     2. THIS handler runs (registered early -> runs last among atexit handlers)
 *        -> sleeps 10ms so a crash that just fired has time to set the flag
 *        -> if flag set, sleeps 2s for the crash handler to finish
 *        -> crash handler's tgkill re-raise terminates the whole process first
 *     3. _exit() -- never reached in the crash case
 *
 * The 50ms unconditional sleep covers the window where the crash fires
 * concurrently with or shortly after this handler starting (flag not yet
 * set on first check). 50ms is negligible for normal shutdown but wide
 * enough to cover crashes triggered late in the teardown sequence.
 * The 2s sleep is a safe upper bound: the crash handler finishes in <500ms;
 * tgkill kills the process well before the 2s expires.
 */
//--------------------------------------------------------------------------------------------------
static void CrashWaitAtExit(void)
{
    /* Unconditional 50ms pause: gives a just-starting crash time to set the
     * flag. 50ms is negligible for normal shutdown but covers crashes that
     * fire late in the teardown sequence (e.g. deep in __cxa_finalize). */
    usleep(50000);

    if (g_crashInProgress)
    {
        /* Crash backtrace is running — wait for the crash handler's tgkill
         * re-raise to terminate the process. 2s is a safe upper bound;
         * in practice the process dies long before this returns. */
        sleep(2);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Alternate signal stack memory. Statically allocated so it is available even when the process
 * heap or the thread's normal stack is corrupted.
 *
 * Size rationale:
 *   ShowStackSignalHandler local vars:  ~400 bytes
 *   backtrace_DumpContextStack frame:   ~100 bytes
 *   DumpContextStack frame:
 *     CrashCtx_t (execRegions[64]):    ~1100 bytes
 *   DumpBacktrace frame:                ~200 bytes
 *   ScanStackForReturnAddrs frame:
 *     rawAddr/rawFp/rawLr[64] + tag:   ~1800 bytes
 *   ParseMapsFile (called many times):
 *     ioBuf[512] + lineBuf[128]:        ~700 bytes per call
 *     Called from LoadExecRegions, FindStackTop, PrintOneAddr (×N frames)
 *     Up to ~10 concurrent calls deep:  ~7000 bytes
 *   PrintOneAddr ResolveCtx.path[128]:  ~200 bytes per call
 *   Function call overhead / alignment: ~2000 bytes
 *   Safety margin (2×):                ×2
 *
 * Total estimate: ~27KB active + 2× margin = ~56KB minimum.
 * We use 65536 (64 KiB) to give comfortable headroom.
 *
 * SIGSTKSZ is intentionally NOT used here: since glibc 2.34 it is no longer
 * a compile-time integer constant — it became a sysconf() result, making it
 * a VLA at file scope and causing a compile error.
 */
//--------------------------------------------------------------------------------------------------
#define ALT_STACK_SIZE  (65536U)    /* 64 KiB */
static uint8_t AltSigStackMem[ALT_STACK_SIZE];

//--------------------------------------------------------------------------------------------------
/**
 * Install the ShowStackSignalHandler to show information and dump stack
 */
//--------------------------------------------------------------------------------------------------
void le_sig_InstallShowStackHandler
(
    void
)
{
    int ret;
    struct sigaction sa;
    char* gdbPtr;
    char* signalShowInfoPtr;

    if ((signalShowInfoPtr = getenv("SIGNAL_SHOW_INFO")))
    {
        if ((0 == strcasecmp(signalShowInfoPtr, "disable")) ||
            (0 == strcasecmp(signalShowInfoPtr, "no")))
        {
            LE_WARN("Handle of SEGV/ILL/BUS/FPE/ABRT and show information disabled");
            return;
        }
    }

    // Install an alternate signal stack so that the crash handler can execute safely even when
    // the faulting thread's normal stack is corrupted or overflowed. Without SA_ONSTACK the
    // handler runs on the already-broken stack and any stack usage (snprintf, open, etc.) will
    // immediately trigger a second fault.
    stack_t altStack;
    altStack.ss_sp    = AltSigStackMem;
    altStack.ss_size  = ALT_STACK_SIZE;
    altStack.ss_flags = 0;
    if (sigaltstack(&altStack, NULL) != 0)
    {
        LE_WARN("Could not install alternate signal stack: %m. "
                "Crash handler may fault on stack-overflow crashes.");
    }

    sa.sa_sigaction = (void (*)(int, siginfo_t *, void *))ShowStackSignalHandler;
    sigemptyset(&sa.sa_mask);
    // SA_RESETHAND  – reset disposition to SIG_DFL after first delivery so that if the handler
    //                 itself faults the kernel delivers the signal with the default action
    //                 (core dump) rather than re-entering our handler recursively.
    // SA_ONSTACK    – deliver on the alternate signal stack installed above.
    // SA_SIGINFO    – pass siginfo_t and ucontext to the handler.
    // SA_NODEFER is intentionally omitted: with SA_RESETHAND the handler is already deregistered
    // before it runs, so there is no need to unblock the signal inside the handler. Keeping the
    // signal blocked during handler execution prevents a second delivery from interrupting the
    // diagnostic output if raise() or a nested fault fires before SA_RESETHAND takes effect.
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    ret = sigaction( SIGSEGV, &sa, NULL );
    if( ret )
    {
        LE_CRIT( "Unable to install signal handler for SIGSEGV : %m\n" );
    }
    ret = sigaction( SIGBUS, &sa, NULL );
    if( ret )
    {
        LE_CRIT( "Unable to install signal handler for SIGBUS : %m\n" );
    }
    ret = sigaction( SIGILL, &sa, NULL );
    if( ret )
    {
        LE_CRIT( "Unable to install signal handler for SIGILL : %m\n" );
    }
    ret = sigaction( SIGFPE, &sa, NULL );
    if( ret )
    {
        LE_CRIT( "Unable to install signal handler for SIGFPE : %m\n" );
    }
    ret = sigaction( SIGABRT, &sa, NULL );
    if( ret )
    {
        LE_CRIT( "Unable to install signal handler for SIGABRT : %m\n" );
    }

    if ((gdbPtr = getenv("GDBSERVER_PORT")))
    {
        if (1 != sscanf(gdbPtr, "%u", &GdbServerPort))
        {
            LE_WARN("Incorrect GDBSERVER_PORT=%s. Discarded...", gdbPtr);
        }
    }

    /* Register the framework atexit handler that waits for an in-progress crash
     * backtrace to complete before _exit() tears down the process.
     * Registered here (early, at app startup) so it runs LAST among all atexit
     * handlers (LIFO order) — after application teardown code that may trigger
     * the crash has already run. */
    atexit(CrashWaitAtExit);
}

//--------------------------------------------------------------------------------------------------
/**
 * Minimal signal handler that exists the application if a SIGTERM has been received.
 */
//--------------------------------------------------------------------------------------------------
static void TermSignalHandler
(
    int sigNum      ///< [IN] The signal that was received.
)
{
    LE_CRIT("Terminated");
    exit(EXIT_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
/**
 * Install a default handler to handle the SIGTERM signal.
 *
 * Called automatically by main().
 */
//--------------------------------------------------------------------------------------------------
void le_sig_InstallDefaultTermHandler
(
    void
)
{
    le_sig_Block(SIGTERM);
    le_sig_SetEventHandler(SIGTERM, TermSignalHandler);
}

//--------------------------------------------------------------------------------------------------
/**
 * The signal event initialization function.  This must be called before any other functions in this
 * module is called.
 */
//--------------------------------------------------------------------------------------------------
void sig_Init
(
    void
)
{
    // Create the memory pools.
    MonitorObjPool = le_mem_CreatePool("SigMonitor", sizeof(MonitorObj_t));
    HandlerObjPool = le_mem_CreatePool("SigHandler", sizeof(HandlerObj_t));

    // Create the pthread local data key.
    LE_ASSERT(pthread_key_create(&SigMonKey, NULL) == 0);
}


//--------------------------------------------------------------------------------------------------
/**
 * Blocks a signal in the calling thread.
 *
 * Signals that an event handler will be set for must be blocked for all threads in the process.  To
 * ensure that the signals are blocked in all threads call this function in the process' first
 * thread, all subsequent threads will inherit the signal mask.
 *
 * @note Does not return on failure.
 */
//--------------------------------------------------------------------------------------------------
void le_sig_Block
(
    int sigNum              ///< [IN] Signal to block.
)
{
    // Check if the calling thread is the main thread.
    pid_t tid = syscall(SYS_gettid);

    LE_FATAL_IF(tid == -1, "Could not get tid of calling thread.  %m.");

    LE_WARN_IF(tid != getpid(), "Blocking signal %d (%s).  Blocking signals not in the main thread \
may result in unexpected behaviour.", sigNum, strsignal(sigNum));

    // Block the signal
    sigset_t sigSet;
    LE_ASSERT(sigemptyset(&sigSet) == 0);
    LE_ASSERT(sigaddset(&sigSet, sigNum) == 0);
    LE_ASSERT(pthread_sigmask(SIG_BLOCK, &sigSet, NULL) == 0);

#ifdef LE_CONFIG_ENABLE_DLT_LOGGING
    // The DLT logging library will start additional working threads. However, the DLT logging gets
    // initialized during legato library loading before main() function, the signal mask can not
    // be applied to those DLT threads if the le_sig_Block() API is called later. This will cause
    // unexpected result in signal handling. To reslove the issue we need to re-initialize the DLT
    // logging so that the DLT threads can inherit the signal blocking mask set by this API.
    log_DltInit();
#endif
}


//--------------------------------------------------------------------------------------------------
/**
 * Set a signal event handler for the calling thread.  Each signal can only have a single event
 * handler.  The most recent event handler set will be called when the signal is received.
 * sigEventHandler can be set to NULL to remove a previously set handler.
 *
 * @param sigNum        Cannot be SIGKILL or SIGSTOP or any program error signals: SIGFPE, SIGILL,
 *                      SIGSEGV, SIGBUS, SIGABRT, SIGIOT, SIGTRAP, SIGEMT, SIGSYS.
 *
 * @note Does not return on failure.
 */
//--------------------------------------------------------------------------------------------------
void le_sig_SetEventHandler
(
    int sigNum,                                 ///< The signal to set the event handler for.  See
                                                ///  parameter documentation in comments above.
    le_sig_EventHandlerFunc_t sigEventHandler   ///< The event handler to call when a signal is
                                                ///  received.
)
{
    // Check parameters.
    if ( (sigNum == SIGKILL) || (sigNum == SIGSTOP) || (sigNum == SIGFPE) || (sigNum == SIGILL) ||
         (sigNum == SIGSEGV) || (sigNum == SIGBUS) || (sigNum == SIGABRT) || (sigNum == SIGIOT) ||
         (sigNum == SIGTRAP) || (sigNum == SIGSYS) )
    {
        LE_FATAL("Signal event handler for %s is not allowed.", strsignal(sigNum));
    }

    // Get the monitor object for this thread.
    MonitorObj_t* monitorObjPtr = pthread_getspecific(SigMonKey);

    if (monitorObjPtr == NULL)
    {
        if (sigEventHandler == NULL)
        {
            // Event handler already does not exist so we don't need to do anything, just return.
            return;
        }
        else
        {
            // Create the monitor object
            monitorObjPtr = le_mem_ForceAlloc(MonitorObjPool);
            monitorObjPtr->handlerObjList = LE_DLS_LIST_INIT;
            monitorObjPtr->fd = -1;
            monitorObjPtr->monitorRef = NULL;

            // Add it to the thread's local data.
            LE_ASSERT(pthread_setspecific(SigMonKey, monitorObjPtr) == 0);
        }
    }

    // See if a handler for this signal already exists.
    HandlerObj_t* handlerObjPtr = FindHandlerObj(sigNum, &(monitorObjPtr->handlerObjList));

    if (handlerObjPtr == NULL)
    {
        if (sigEventHandler == NULL)
        {
            // Event handler already does not exist so we don't need to do anything, just return.
            return;
        }
        else
        {
            // Create the handler object.
            handlerObjPtr = le_mem_ForceAlloc(HandlerObjPool);

            // Set the handler.
            handlerObjPtr->link = LE_DLS_LINK_INIT;
            handlerObjPtr->handler = sigEventHandler;
            handlerObjPtr->sigNum = sigNum;

            // Add the handler object to the list.
            le_dls_Queue(&(monitorObjPtr->handlerObjList), &(handlerObjPtr->link));
        }
    }
    else
    {
        if (sigEventHandler == NULL)
        {
            // Remove the handler object from the list.
            le_dls_Remove(&(monitorObjPtr->handlerObjList), &(handlerObjPtr->link));
        }
        else
        {
            // Just update the handler.
            handlerObjPtr->handler = sigEventHandler;
        }
    }

    // Recreate the signal mask.
    sigset_t sigSet;
    LE_ASSERT(sigemptyset(&sigSet) == 0);

    le_dls_Link_t* handlerLinkPtr = le_dls_Peek(&(monitorObjPtr->handlerObjList));
    while (handlerLinkPtr != NULL)
    {
        HandlerObj_t* handlerObjPtr = CONTAINER_OF(handlerLinkPtr, HandlerObj_t, link);

        LE_ASSERT(sigaddset(&sigSet, handlerObjPtr->sigNum) == 0);

        handlerLinkPtr = le_dls_PeekNext(&(monitorObjPtr->handlerObjList), handlerLinkPtr);
    }

    // Update or create the signal fd.
    monitorObjPtr->fd = signalfd(monitorObjPtr->fd, &sigSet, SFD_NONBLOCK);

    if (monitorObjPtr->fd == -1)
    {
        LE_FATAL("Could not set signal event handler: %m");
    }

    // Create a monitor fd if it doesn't already exist.
    if (monitorObjPtr->monitorRef == NULL)
    {
        // Create the monitor name using SIG_STR + thread name.
        char monitorName[LIMIT_MAX_THREAD_NAME_BYTES + sizeof(SIG_STR)] = SIG_STR;

        LE_ASSERT(le_utf8_Copy(monitorName + sizeof(SIG_STR),
                               le_thread_GetMyName(),
                               LIMIT_MAX_THREAD_NAME_BYTES + sizeof(SIG_STR),
                               NULL) == LE_OK);

        // Create the monitor.
        monitorObjPtr->monitorRef = le_fdMonitor_Create(monitorName,
                                                        monitorObjPtr->fd,
                                                        OurSigHandler,
                                                        POLLIN);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Removes all signal event handlers for the calling thread and cleans up any resources used for
 * signal events.  This should be called before the thread exits.
 */
//--------------------------------------------------------------------------------------------------
void le_sig_DeleteAll
(
    void
)
{
    // Get the monitor object for this thread.
    MonitorObj_t* monitorObjPtr = pthread_getspecific(SigMonKey);

    if (monitorObjPtr != NULL)
    {
        // Delete the monitor.
        le_fdMonitor_Delete(monitorObjPtr->monitorRef);

        // Close the file descriptor.
        while(1)
        {
            int n = close(monitorObjPtr->fd);

            if (n == 0)
            {
                break;
            }
            else if (errno != EINTR)
            {
                LE_FATAL("Could not close file descriptor.");
            }
        }

        // Remove all handler objects from the list and free them.
        le_dls_Link_t* handlerLinkPtr = le_dls_Pop(&(monitorObjPtr->handlerObjList));

        while (handlerLinkPtr != NULL)
        {
            le_mem_Release( CONTAINER_OF(handlerLinkPtr, HandlerObj_t, link) );

            handlerLinkPtr = le_dls_Pop(&(monitorObjPtr->handlerObjList));
        }

        // Release the monitor object.
        le_mem_Release(monitorObjPtr);
    }
}
