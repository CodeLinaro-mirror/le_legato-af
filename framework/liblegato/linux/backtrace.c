//--------------------------------------------------------------------------------------------------
/** @file backtrace.c
 *
 * Utility function for printing a backtrace.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "signals.h"

#include <setjmp.h>
#include <ucontext.h>

#if LE_CONFIG_ENABLE_SEGV_HANDLER

static sigjmp_buf SigEnv;

static int SegvFired;

static __attribute__((unused)) void SigSegVHandler( int signum )
{
    (void)signum;
    SIG_WRITE("[...]\n", 6);
    if (!SegvFired)
    {
        SegvFired = 1;
        siglongjmp(SigEnv, 1);
    }
    /* Second fault while unwinding: let the default action terminate cleanly */
}

#endif // LE_CONFIG_ENABLE_SEGV_HANDLER

#if defined(__arm__)

static inline void DumpContextStack(const void *infoPtr, int skip, char *buf, size_t bufLen,
                                    pid_t crashTid)
{
    (void)crashTid; /* ARM path does not need crashTid — FP walk uses ucontext directly */
    const struct sigcontext     *ctxPtr = (const struct sigcontext *)
                                    &(((const ucontext_t *) infoPtr)->uc_mcontext);
    int                          addr = ctxPtr->arm_pc;
    
    int                         *crashFP = (int*)(uintptr_t)ctxPtr->arm_fp;
    int                         *frame   = crashFP;

#if LE_CONFIG_ENABLE_SEGV_HANDLER
    struct sigaction sa, saveSaSegV;
    int ret;

    sa.sa_sigaction = (void (*)(int, siginfo_t *, void *))SigSegVHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    ret = sigaction( SIGSEGV, &sa, &saveSaSegV);
    if (ret)
    {
        SIG_WRITE("sigaction failed\n", 17);
    }

    if (0 == sigsetjmp(SigEnv, 1))
#endif // LE_CONFIG_ENABLE_SEGV_HANDLER
    {
        SIG_WRITE("PC at ", 6);
        SigWriteHex32((uint32_t)ctxPtr->arm_pc);
        SIG_WRITE("\n", 1);
        SIG_WRITE("LR at ", 6);
        SigWriteHex32((uint32_t)ctxPtr->arm_lr);
        SIG_WRITE(" [", 2);
        SigWriteHexPtr(frame);
        SIG_WRITE("]\n", 2);
#if LE_CONFIG_ENABLE_SEGV_HANDLER
        if (frame)
#else
        // Bound the walk to a 1 MB window above the crash FP to avoid runaway
        // reads on a corrupted stack. The upper bound is relative to crashFP
        // (the crashed thread's FP), not the altstack.
        if ((frame) && (frame <= (crashFP + 1024*1024)) && (frame >= crashFP))
#endif // LE_CONFIG_ENABLE_SEGV_HANDLER
        {
            frame = (int*)*(frame-1);
        }
        for ( ; ; )
        {

            if ((frame < (int*)0x1000) || (frame > (crashFP + 1024*1024)) || (frame < crashFP))
            {
                break;
            }
            addr = *(frame);
            SIG_WRITE("LR at ", 6);
            SigWriteHex32((uint32_t)addr);
            SIG_WRITE(" [", 2);
            SigWriteHexPtr(frame);
            SIG_WRITE("]\n", 2);
            frame = (int *)*(frame-1);
        }
    }
#if LE_CONFIG_ENABLE_SEGV_HANDLER
    else
    {
        SIG_WRITE("Abort while dumping the backtrace\n", 34);
    }

    ret = sigaction( SIGSEGV, &sa, NULL );
    if (ret)
    {
        SIG_WRITE("sigaction failed\n", 17);
    }

    if (0 == sigsetjmp(SigEnv, 1))
#endif // LE_CONFIG_ENABLE_SEGV_HANDLER
    {
        /* Register dump — each "rNN  VVVVVVVV" field uses SigWriteHex32, no snprintf. */
        SIG_WRITE("r0  ", 4);  SigWriteHex32((uint32_t)ctxPtr->arm_r0);
        SIG_WRITE(" r1  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r1);
        SIG_WRITE(" r2  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r2);
        SIG_WRITE(" r3  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r3);
        SIG_WRITE(" r4  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r4);
        SIG_WRITE("  r5  ", 6);SigWriteHex32((uint32_t)ctxPtr->arm_r5);
        SIG_WRITE("\n", 1);
        SIG_WRITE("r6  ", 4);  SigWriteHex32((uint32_t)ctxPtr->arm_r6);
        SIG_WRITE(" r7  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r7);
        SIG_WRITE(" r8  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r8);
        SIG_WRITE(" r9  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r9);
        SIG_WRITE(" r10 ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_r10);
        SIG_WRITE(" cpsr ", 6);SigWriteHex32((uint32_t)ctxPtr->arm_cpsr);
        SIG_WRITE("\n", 1);
        SIG_WRITE("fp  ", 4);  SigWriteHex32((uint32_t)ctxPtr->arm_fp);
        SIG_WRITE(" ip  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_ip);
        SIG_WRITE(" sp  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_sp);
        SIG_WRITE(" lr  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_lr);
        SIG_WRITE(" pc  ", 5); SigWriteHex32((uint32_t)ctxPtr->arm_pc);
        SIG_WRITE("\n", 1);
        SIG_WRITE("STACK ", 6);SigWriteHex32((uint32_t)ctxPtr->arm_sp);
        SIG_WRITE(", FRAME ", 8);SigWriteHex32((uint32_t)ctxPtr->arm_fp);
        SIG_WRITE("\n", 1);
        for (addr = 0, frame = (int*)ctxPtr->arm_sp-32;
#if LE_CONFIG_ENABLE_SEGV_HANDLER
             addr < 1024;
#else
             addr < 256;
#endif // LE_CONFIG_ENABLE_SEGV_HANDLER
             addr += 8, frame += 8)
        {
            SigWriteHexPtr(frame);
            SIG_WRITE(": ", 2);
            SigWriteHex32((uint32_t)frame[0]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[1]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[2]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[3]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[4]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[5]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[6]); SIG_WRITE(" ", 1);
            SigWriteHex32((uint32_t)frame[7]); SIG_WRITE("\n", 1);
        }
    }
#if LE_CONFIG_ENABLE_SEGV_HANDLER
    else
    {
        SIG_WRITE("Abort while dumping the stack\n", 30);
    }

    (void)sigaction( SIGSEGV, &saveSaSegV, NULL );
#endif // LE_CONFIG_ENABLE_SEGV_HANDLER
}

#else /* if not __arm__ */

/* Parse one line of /proc/<pid>/maps. Returns 1 on success, 0 on failure. */
static int ParseMapsLine
(
    const char *line,
    uintptr_t  *start,
    uintptr_t  *end,
    int        *exec
)
{
    const char *p = line;
    uintptr_t s = 0, e = 0;
    int i;
    for (i = 0; i < 16 && *p; i++, p++)
    {
        char c = *p;
        if      (c >= '0' && c <= '9') { s = (s << 4) | (uintptr_t)(c - '0'); }
        else if (c >= 'a' && c <= 'f') { s = (s << 4) | (uintptr_t)(c - 'a' + 10); }
        else if (c >= 'A' && c <= 'F') { s = (s << 4) | (uintptr_t)(c - 'A' + 10); }
        else break;
    }
    if (*p != '-') { return 0; }
    p++;
    for (i = 0; i < 16 && *p; i++, p++)
    {
        char c = *p;
        if      (c >= '0' && c <= '9') { e = (e << 4) | (uintptr_t)(c - '0'); }
        else if (c >= 'a' && c <= 'f') { e = (e << 4) | (uintptr_t)(c - 'a' + 10); }
        else if (c >= 'A' && c <= 'F') { e = (e << 4) | (uintptr_t)(c - 'A' + 10); }
        else break;
    }
    if (*p != ' ') { return 0; }
    p++;
    *exec  = (p[2] == 'x') ? 1 : 0;
    *start = s;
    *end   = e;
    return 1;
}

/* Executable region table entry. */
#define MAX_EXEC_REGIONS 64
typedef struct { uintptr_t start; uintptr_t end; } ExecRegion_t;

/* Build /proc/<id>/<suffix> path into pathBuf. Async-signal-safe. */
static void BuildProcPath
(
    char       *pathBuf,
    const char *suffix,   /* e.g. "/maps" or "/stat" */
    unsigned long id      /* pid or tid */
)
{
    const char prefix[] = "/proc/";
    char idStr[21];
    int  ii = (int)sizeof(idStr) - 1;
    int  pl;
    idStr[ii] = '\0';
    if (id == 0) { idStr[--ii] = '0'; }
    else { while (id > 0) { idStr[--ii] = (char)('0' + id % 10); id /= 10; } }
    pl = (int)(sizeof(prefix) - 1);
    memcpy(pathBuf, prefix, (size_t)pl);
    int idLen = (int)sizeof(idStr) - 1 - ii;
    memcpy(pathBuf + pl, idStr + ii, (size_t)idLen);
    pl += idLen;
    int sfxLen = 0;
    while (suffix[sfxLen]) { sfxLen++; }
    memcpy(pathBuf + pl, suffix, (size_t)(sfxLen + 1)); /* include NUL */
}

/* Bulk-read /proc/self/maps, call cb() per line. cb returns 1 to stop. */
typedef int (*MapsLineCallback)(const char *line, void *ctx);

static void ParseMapsFile
(
    MapsLineCallback  cb,
    void             *cbCtx
)
{
    /* /proc/self/maps works regardless of PID namespace (sandbox-safe). */
    const char path[] = "/proc/self/maps";

    int fd = open(path, O_RDONLY);
    if (fd < 0) { return; }

    char ioBuf[512];   /* bulk read buffer */
    char lineBuf[128]; /* accumulates one line */
    int  lineLen = 0;
    int  stop    = 0;

    while (!stop)
    {
        int rc = read(fd, ioBuf, sizeof(ioBuf));
        if (rc <= 0) { break; }
        int i;
        for (i = 0; i < rc && !stop; i++)
        {
            char c = ioBuf[i];
            if (c == '\n')
            {
                lineBuf[lineLen] = '\0';
                if (cb(lineBuf, cbCtx)) { stop = 1; }
                lineLen = 0;
            }
            else if (lineLen < (int)sizeof(lineBuf) - 1)
            {
                lineBuf[lineLen++] = c;
            }
        }
    }
    /* flush partial last line (no trailing newline) */
    if (!stop && lineLen > 0)
    {
        lineBuf[lineLen] = '\0';
        cb(lineBuf, cbCtx);
    }
    close(fd);
}

/* Callback context for LoadExecRegions */
typedef struct { ExecRegion_t *regions; int max; int count; } LoadExecCtx;
static int LoadExecCb(const char *line, void *ctx)
{
    LoadExecCtx *c = (LoadExecCtx *)ctx;
    if (c->count >= c->max) { return 1; } /* stop — table full */
    uintptr_t s, e; int x;
    if (ParseMapsLine(line, &s, &e, &x) && x)
    { c->regions[c->count].start = s; c->regions[c->count].end = e; c->count++; }
    return 0;
}

/* Load all executable regions from /proc/self/maps. Returns count. */
static int LoadExecRegions
(
    ExecRegion_t *regions,
    int           maxRegions
)
{
    LoadExecCtx ctx = { regions, maxRegions, 0 };
    ParseMapsFile(LoadExecCb, &ctx);
    return ctx.count;
}

/* Callback context for FindStackTop */
typedef struct { uintptr_t spVal; uintptr_t stackTop; int found; } FindStackCtx;
static int FindStackCb(const char *line, void *ctx)
{
    FindStackCtx *c = (FindStackCtx *)ctx;
    uintptr_t s, e; int x;
    if (ParseMapsLine(line, &s, &e, &x) && c->spVal >= s && c->spVal < e)
    { c->stackTop = e; c->found = 1; return 1; } /* stop — found it */
    return 0;
}

/* Find the maps region containing spVal; set *stackTop to its end. */
static int FindStackTop
(
    uintptr_t   spVal,
    uintptr_t  *stackTop
)
{
    FindStackCtx ctx = { spVal, 0, 0 };
    ParseMapsFile(FindStackCb, &ctx);
    if (ctx.found) { *stackTop = ctx.stackTop; }
    return ctx.found;
}

/* Read startstack (field 28) and kstkesp (field 29) from /proc/<tid>/stat. */
static int ReadThreadStackInfo
(
    pid_t      tid,
    uintptr_t *startStack,  ///< [OUT] field 28: bottom of stack (highest addr)
    uintptr_t *kstkesp      ///< [OUT] field 29: current SP (may be altstack when in handler)
)
{
    char path[48];
    int  fd;
    char buf[512];
    int  rc;
    BuildProcPath(path, "/stat", (unsigned long)tid);

    fd = open(path, O_RDONLY);
    if (fd < 0) { return 0; }
    rc = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (rc <= 0) { return 0; }
    buf[rc] = '\0';

    /* Skip past comm field '(...)', then count fields to reach 28=startstack, 29=kstkesp. */
    const char *p = buf;
    while (*p && *p != ')') { p++; }
    if (*p == ')') { p++; }

    /* We are now just before field 3. Count spaces to reach fields 28 and 29. */
    int field = 3;
    *startStack = 0;
    *kstkesp    = 0;

    while (*p && field <= 29)
    {
        if (*p == ' ')
        {
            field++;
            p++;
            if (field == 28 || field == 29)
            {
                uintptr_t val = 0;
                while (*p >= '0' && *p <= '9')
                {
                    val = val * 10 + (uintptr_t)(*p - '0');
                    p++;
                }
                if (field == 28) { *startStack = val; }
                else             { *kstkesp    = val; }
            }
        }
        else
        {
            p++;
        }
    }
    return (*startStack != 0) ? 1 : 0;
}

/* Return 1 if addr is in any executable region. */
static int IsExecAddr
(
    uintptr_t          addr,
    const ExecRegion_t *regions,
    int                 nRegions
)
{
    int i;
    for (i = 0; i < nRegions; i++)
    {
        if (addr >= regions[i].start && addr < regions[i].end) { return 1; }
    }
    return 0;
}

/* Print one address as lib(+0xOFFSET)[0xADDR]. Async-signal-safe. */
typedef struct { uintptr_t addr; uintptr_t regionStart; char path[128]; int found; } ResolveCtx;
static int ResolveCb(const char *line, void *ctx)
{
    ResolveCtx *c = (ResolveCtx *)ctx;
    uintptr_t s, e; int x;
    if (!ParseMapsLine(line, &s, &e, &x)) { return 0; }
    if (c->addr < s || c->addr >= e)      { return 0; }
    /* Found the region — record start and extract path (field 6, after 5 spaces) */
    c->regionStart = s;
    c->found = 1;
    /* Skip 5 space-delimited fields to reach the path */
    const char *p = line;
    int spaces = 0;
    while (*p && spaces < 5) { if (*p == ' ') { spaces++; } p++; }
    /* Skip any extra spaces before the path */
    while (*p == ' ') { p++; }
    /* Copy path into buffer */
    int i = 0;
    while (*p && *p != '\n' && i < (int)sizeof(c->path) - 1) { c->path[i++] = *p++; }
    c->path[i] = '\0';
    return 1; /* stop */
}

static void PrintOneAddr
(
    uintptr_t           addr,
    const ExecRegion_t *regions,
    int                 nRegions,
    int                 noNewline   /* 1 = caller will write the newline */
)
{
    (void)regions;
    (void)nRegions;
    ResolveCtx rctx;
    rctx.addr        = addr;
    rctx.regionStart = 0;
    rctx.path[0]     = '\0';
    rctx.found       = 0;
    ParseMapsFile(ResolveCb, &rctx);
    if (rctx.found && rctx.path[0] != '\0')
    {
        /* Print: /path/to/lib.so(+0xOFFSET)[0xADDR] */
        SIG_WRITE("  ", 2);
        SigWriteStr(rctx.path);
        SIG_WRITE("(+", 2);
        SigWriteHexOffset(addr - rctx.regionStart);
        SIG_WRITE(")[", 2);
        SigWriteHexPtr((void *)addr);
        SIG_WRITE("]", 1);
    }
    else
    {
        /* No mapping found — print raw address */
        SIG_WRITE("  [", 3);
        SigWriteHexPtr((void *)addr);
        SIG_WRITE("]", 1);
    }
    if (!noNewline)
    {
        SIG_WRITE("\n", 1);
    }
}

/* Print array of addresses via PrintOneAddr. */
static void PrintAddrArray
(
    void *const        *frames,
    int                 nFrames,
    const ExecRegion_t *regions,
    int                 nRegions
)
{
    int i;
    for (i = 0; i < nFrames; i++)
    {
        PrintOneAddr((uintptr_t)frames[i], regions, nRegions, 0);
    }
}

/* Bottom-up stack scan: find anchor, collect candidates, validate linkage. */
static void ScanStackForReturnAddrs
(
    uintptr_t           spVal,
    uintptr_t           stackTop,
    uintptr_t           ssBase,
    uintptr_t           ssTop,
    const ExecRegion_t *regions,
    int                 nRegions
)
{
    int        nFrames = 0;
    uintptr_t  ptr;
    uintptr_t  anchor = 0;
    (void)ssTop;

    if (spVal < 0x1000 || stackTop <= spVal) { return; }
    if (ssBase != 0 && spVal < ssBase) { spVal = ssBase; }

    /* Phase A: find start_thread anchor (fp=0, lr=exec) in full SP->stackTop range. */
    {
        uintptr_t scanTop  = stackTop;
        uintptr_t scanFloor = spVal;

        /* Scan top-down at 8-byte stride looking for the start_thread frame:
         * a pair {fp=0, lr=exec} at any 8-byte aligned address. */
        ptr = (scanTop - 16) & ~(uintptr_t)0x7;
        while (ptr >= scanFloor)
        {
            uintptr_t fp_cand = *(const uintptr_t *)(ptr + 0);
            uintptr_t lr_cand = *(const uintptr_t *)(ptr + sizeof(uintptr_t));
            if (fp_cand == 0 && IsExecAddr(lr_cand, regions, nRegions)
                && (lr_cand & 0xFFF) != 0)
            {
                anchor = ptr;
                break;
            }
            if (ptr < scanFloor + 8) { break; }
            ptr -= 8;
        }
    }

    if (anchor == 0)
    {
        SIG_WRITE("  (start_thread anchor not found)\n", 35);
        return;
    }

    SIG_WRITE("  anchor=start_thread frame @ ", 30);
    SigWriteHexPtr((void *)anchor);
    SIG_WRITE("\n", 1);

    /* Phase B: collect candidates (Pass 1) then validate linkage (Pass 2).
     * Markers: (none)=linked  ? stale=fp not in list  ? dup lr=duplicate LR
     */

#define MAX_SCAN_FRAMES 64
#define SCAN_LINKED  0
#define SCAN_NOFP    1
#define SCAN_DUP_LR  2
    uintptr_t rawAddr[MAX_SCAN_FRAMES]; /* slot address */
    uintptr_t rawFp[MAX_SCAN_FRAMES];   /* saved_fp     */
    uintptr_t rawLr[MAX_SCAN_FRAMES];   /* saved_lr     */
    int        rawTag[MAX_SCAN_FRAMES]; /* LINKED/NOFP/DUP_LR */

    {
        /* Anchor is index 0: saved_fp=0 (chain terminator). */
        uintptr_t saved_lr = *(const uintptr_t *)(anchor + sizeof(uintptr_t));
        if (IsExecAddr(saved_lr, regions, nRegions) && nFrames < MAX_SCAN_FRAMES)
        {
            rawAddr[nFrames] = anchor;
            rawFp[nFrames]   = 0;
            rawLr[nFrames]   = saved_lr;
            nFrames++;
        }

        /* Pass 1: scan downward from anchor-8 toward spVal, one word (8 bytes)
         * at a time. Each aligned 8-byte word is a potential return address.
         * We collect every word that is an executable address with non-zero
         * page offset — regardless of what the preceding word (fp slot) holds.
         * Scanning at 8-byte stride (not 16) catches tightly-packed return
         * addresses from inlined / -fomit-frame-pointer frames. */
        if (anchor >= spVal + 8)
        {
            ptr = anchor - 8;
            while (ptr >= spVal && nFrames < MAX_SCAN_FRAMES)
            {
                uintptr_t word = *(const uintptr_t *)ptr;

                /* Skip page-aligned addresses — load/section addresses, not
                 * return addresses. Real BL/BLR return sites are never at a
                 * 4KB page boundary. */
                if (IsExecAddr(word, regions, nRegions)
                    && (word & 0xFFF) != 0)
                {
                    /* Record the word before this one as the "fp" slot for
                     * linkage checking in Pass 2. */
                    uintptr_t prev_word = (ptr >= spVal + 8)
                        ? *(const uintptr_t *)(ptr - 8) : 0;
                    rawAddr[nFrames] = ptr;
                    rawFp[nFrames]   = prev_word;
                    rawLr[nFrames]   = word;
                    nFrames++;
                }
                if (ptr < spVal + 8) { break; }
                ptr -= 8;
            }
        }
    }

    /* Pass 2: validate linkage oldest→newest. Anchor (index 0) always trusted.
     * LINKED  = saved_fp chains back to a known slot above (has frame pointer).
     * NOFP    = saved_fp does not chain, but LR is executable — likely a real
     *           frame compiled with -fomit-frame-pointer.
     * DUP_LR  = duplicate LR vs a previously LINKED frame — likely noise.
     */
    {
        int i;
        rawTag[0] = SCAN_LINKED;

        for (i = 1; i < nFrames; i++)
        {
            int j;
            int isDup = 0;

            /* Dup-LR check: only against previously LINKED frames. */
            for (j = 0; j < i; j++)
            {
                if (rawTag[j] == SCAN_LINKED && rawLr[j] == rawLr[i])
                {
                    isDup = 1;
                    break;
                }
            }
            if (isDup)
            {
                rawTag[i] = SCAN_DUP_LR;
                continue;
            }

            /* Linkage check: the word immediately before this LR word must
             * equal the address of another collected LR word (i.e. a valid
             * saved-FP pointing into our candidate list). */
            {
                int isLinked = 0;
                for (j = 0; j < i; j++)
                {
                    if (rawFp[i] == rawAddr[j])
                    {
                        isLinked = 1;
                        break;
                    }
                }
                rawTag[i] = isLinked ? SCAN_LINKED : SCAN_NOFP;
            }
        }
    }

    {
        int i;
        int nLinked = 0;
        int nStale  = 0;
        int nDup    = 0;

        for (i = nFrames - 1; i >= 0; i--)
        {
            /* noNewline=1: append marker before writing \n ourselves. */
            PrintOneAddr(rawLr[i], regions, nRegions, 1);

            /* Append validation marker. */
            switch (rawTag[i])
            {
                case SCAN_LINKED:  nLinked++;                               break;
                case SCAN_NOFP:    SIG_WRITE("  ? nofp",   8); nStale++;    break;
                case SCAN_DUP_LR:  SIG_WRITE("  ? dup lr", 10); nDup++;     break;
                default:                                                     break;
            }

            SIG_WRITE("\n", 1);
        }

        SIG_WRITE("\n", 1); /* blank line after frame list */
        SIG_WRITE("  (", 3);
        SigWriteULong((unsigned long)nFrames);
        SIG_WRITE(" frames: ", 9);
        SigWriteULong((unsigned long)nLinked);
        SIG_WRITE(" linked, ", 9);
        SigWriteULong((unsigned long)nStale);
        SIG_WRITE(" nofp, ", 7);
        SigWriteULong((unsigned long)nDup);
        SIG_WRITE(" dup)\n", 6);
    }
#undef MAX_SCAN_FRAMES
#undef SCAN_LINKED
#undef SCAN_NOFP
#undef SCAN_DUP_LR
}

/* Context block shared between DumpRegs and DumpBacktrace. */
typedef struct
{
    const void   *infoPtr;               /* original ucontext pointer          */
    pid_t         crashTid;
    void         *pcPtr, *spPtr, *lrPtr, *fpPtr;
    int           regsKnown;
    ExecRegion_t  execRegions[MAX_EXEC_REGIONS];
    int           nExecRegions;
    uintptr_t     ssBase;                /* uc_stack.ss_sp                     */
    uintptr_t     ssTop;                 /* ssBase + uc_stack.ss_size          */
    uintptr_t     mapsTop;               /* end of mapped region holding SP    */
    uintptr_t     fpChainTop;            /* highest FP seen during chain walk  */
    int           fpChainComplete;       /* 1 = chain terminated naturally     */
} CrashCtx_t;


static void DumpRegs(const CrashCtx_t *ctx)
{
    if (!ctx->regsKnown) { return; }

    SIG_WRITE("PC: ", 4);  SigWriteHexPtr(ctx->pcPtr);
    SIG_WRITE(" SP: ", 5); SigWriteHexPtr(ctx->spPtr);
    SIG_WRITE(" LR: ", 5); SigWriteHexPtr(ctx->lrPtr);
    SIG_WRITE(" FP: ", 5); SigWriteHexPtr(ctx->fpPtr);
    SIG_WRITE("\n", 1);
}


static void DumpBacktrace(CrashCtx_t *ctx)
{
    uintptr_t scanSP  = 0;
    uintptr_t stackTop = 0;

    SIG_WRITE("\n", 1); /* blank line after register line */


    if (ctx->spPtr != NULL && (uintptr_t)ctx->spPtr >= 0x1000)
    {
        uintptr_t sp = (uintptr_t)ctx->spPtr;
        /* Under SA_ONSTACK, uc_stack holds the ALT-STACK bounds.
         * sp in [ssBase, ssTop) → stack overflow: handler is on the alt-stack,
         *   normal stack is trashed — scan from ssBase (whole normal stack).
         * sp outside [ssBase, ssTop) → normal crash: SP is on the thread
         *   stack — use it directly as the scan floor. */
        if (ctx->ssBase != 0 && ctx->ssTop != 0
            && sp >= ctx->ssBase && sp < ctx->ssTop)
        {
            /* Stack overflow: scan the entire normal stack from its base. */
            scanSP = ctx->ssBase;
        }
        else
        {
            /* Normal crash (or no uc_stack info): use crash SP directly. */
            scanSP = sp;
        }
    }

    if (ctx->ssBase != 0 && ctx->ssTop != 0)
    {
        stackTop = ctx->ssTop;
        if (scanSP == 0) { scanSP = ctx->ssBase; }
    }

    if (ctx->mapsTop != 0 && ctx->mapsTop > scanSP)
    {
        stackTop = ctx->mapsTop;
    }

    /* Source 4: /proc/<tid>/stat startstack — last resort when maps lookup
     * failed (e.g. maps file unreadable) and we still have no stackTop. */
    if (stackTop == 0 || scanSP == 0)
    {
        uintptr_t statStartStack = 0, statKstkesp = 0;
        if (ReadThreadStackInfo(ctx->crashTid, &statStartStack, &statKstkesp)
            && statStartStack >= 0x1000)
        {
            if (stackTop == 0) { stackTop = statStartStack; }
            if (scanSP  == 0)
            {
                scanSP = (stackTop > 8*1024*1024) ? stackTop - 8*1024*1024 : 0x1000;
            }
        }
    }


    if (ctx->regsKnown && ctx->fpPtr != NULL && (uintptr_t)ctx->fpPtr >= 0x1000)
    {
        void  *retSp[32];
        size_t nRetSp = 0;
        void **framePtr    = (void **)ctx->fpPtr;
        void **preFramePtr = NULL;
        void **fpWalkLimit;
        if (ctx->mapsTop != 0)
            fpWalkLimit = (void **)ctx->mapsTop;
        else if (ctx->ssTop != 0)
            fpWalkLimit = (void **)ctx->ssTop;
        else if (ctx->ssBase != 0)
            /* ssTop unavailable but ssBase known: use ssBase + default 8MB stack */
            fpWalkLimit = (void **)(ctx->ssBase + 8 * 1024 * 1024);
        else
            /* Last resort: cap at 1MB above fpPtr — better than unbounded */
            fpWalkLimit = (void **)((uintptr_t)ctx->fpPtr + 1024 * 1024);

        SIG_WRITE("--- BACKTRACE: FP CHAIN (newest to oldest) ---\n", 47);
        /* On AArch64, PC=crash site, LR=return address of the crashing call.
         * LR is the newest real frame — insert it first, before FP walk results.
         * PC is included only if non-NULL (non-zero crash address). */
        if (ctx->pcPtr != NULL) { retSp[nRetSp++] = ctx->pcPtr; }
        if (ctx->lrPtr != NULL && nRetSp < 32) { retSp[nRetSp++] = ctx->lrPtr; }
        while (framePtr != NULL
               && (uintptr_t)framePtr >= 0x1000
               && framePtr < fpWalkLimit
               && framePtr > preFramePtr
               && nRetSp < 32)
        {
            void *savedLr = *(framePtr + 1);
            /* If saved_lr is not in any executable region the chain is
             * corrupt — stop here rather than collecting garbage. */
            if (!IsExecAddr((uintptr_t)savedLr, ctx->execRegions, ctx->nExecRegions))
            {
                break;
            }
            retSp[nRetSp++] = savedLr;
            preFramePtr = framePtr;
            framePtr    = (void **)(*framePtr);
        }
        if (preFramePtr != NULL)
        {
            ctx->fpChainTop = (uintptr_t)preFramePtr + sizeof(void *) * 2;

            int naturalEnd = (framePtr == NULL
                              || (uintptr_t)framePtr < 0x1000
                              || framePtr >= fpWalkLimit);
            if (naturalEnd && nRetSp < 32) { ctx->fpChainComplete = 1; }
        }

        /* Write raw: line NOW — before any maps access, unconditionally.
         * This is the safety net: even if symbol resolution faults and
         * siglongjmp fires, these addresses are already in the log. */
        {
            size_t ri;
            SIG_WRITE("  raw:", 6);
            for (ri = 0; ri < nRetSp; ri++)
            {
                SIG_WRITE(" ", 1);
                SigWriteHexPtr(retSp[ri]);
            }
            SIG_WRITE("\n", 1);
        }

#if LE_CONFIG_ENABLE_SEGV_HANDLER
        {
            struct sigaction sa, saveSaSegV;
            sa.sa_sigaction = (void (*)(int, siginfo_t *, void *))SigSegVHandler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_SIGINFO;
            SegvFired = 0;
            if (sigaction(SIGSEGV, &sa, &saveSaSegV))
            { SIG_WRITE("sigaction failed\n", 17); }
            if (0 == sigsetjmp(SigEnv, 1))
            {
#endif
                /* Symbol resolution */
                if (nRetSp > 0)
                {
                    PrintAddrArray(retSp, (int)nRetSp, ctx->execRegions, ctx->nExecRegions);
                }
                SIG_WRITE("\n", 1);
                SIG_WRITE("--- FP CHAIN END (", 18);
                SigWriteULong((unsigned long)nRetSp);
                if (ctx->fpChainComplete)
                    { SIG_WRITE(" frames, complete) ---\n", 23); }
                else if (nRetSp >= 32)
                    { SIG_WRITE(" frames, truncated at cap) ---\n", 31); }
                else
                    { SIG_WRITE(" frames, chain broken) ---\n", 27); }

                /* Stack scan */
                {
                    uintptr_t scanTop = (ctx->mapsTop != 0) ? ctx->mapsTop : stackTop;
                    SIG_WRITE("\n", 1);
                    SIG_WRITE("--- BACKTRACE: STACK SCAN (newest to oldest) ---\n", 49);
                    SIG_WRITE("  SP=", 5); SigWriteHexPtr((void *)scanSP);
                    SIG_WRITE(" top=", 5); SigWriteHexPtr((void *)scanTop);
                    SIG_WRITE("\n", 1);
                    if (scanSP >= 0x1000 && scanTop > scanSP)
                    {
                        ScanStackForReturnAddrs(scanSP, scanTop,
                                                 ctx->ssBase, ctx->ssTop,
                                                 ctx->execRegions, ctx->nExecRegions);
                    }
                    else
                    {
                        SIG_WRITE("  (skipped: invalid SP or top)\n", 31);
                    }
                    SIG_WRITE("--- STACK SCAN END ---\n", 23);
                }
#if LE_CONFIG_ENABLE_SEGV_HANDLER
            }
            else { SIG_WRITE("Catching SEGV while dumping the backtrace\n", 42); }
            (void)sigaction(SIGSEGV, &saveSaSegV, NULL);
        }
#endif

        /* Raw hex dump of the normal stack — written LAST, after all critical
         * output. Skipped when on altstack (spPtr outside [ssBase,ssTop)).
         * 512 bytes = 8 rows × 8 words on aarch64 (~1.2 KB text). */
        if (ctx->spPtr != NULL && (uintptr_t)ctx->spPtr >= 0x1000)
        {
            int onAltstack = (ctx->ssBase != 0 && ctx->ssTop != 0
                              && ((uintptr_t)ctx->spPtr < ctx->ssBase
                                  || (uintptr_t)ctx->spPtr >= ctx->ssTop));
            if (!onAltstack)
            {
                uintptr_t dumpBase = (uintptr_t)ctx->spPtr;
                uintptr_t dumpTop  = dumpBase + 512;
                uintptr_t *daddr   = (uintptr_t *)dumpBase;
                uintptr_t *dend    = (uintptr_t *)dumpTop;
                SIG_WRITE("\n--- STACK HEX (SP, 512 bytes) ---\n", 35);
                while (daddr + 8 <= dend)
                {
                    SigWriteHexPtr((void *)daddr);    SIG_WRITE(": ", 2);
                    SigWriteHexPtr((void *)daddr[0]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[1]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[2]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[3]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[4]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[5]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[6]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[7]); SIG_WRITE("\n", 1);
                    daddr += 8;
                }
                SIG_WRITE("--- STACK HEX END ---\n", 22);
            }
        }
    }
    else if (ctx->regsKnown && ctx->lrPtr != NULL)
    {
        void  *retSp[2];
        size_t nRetSp = 0;
        if (ctx->pcPtr != NULL) { retSp[nRetSp++] = ctx->pcPtr; }
        retSp[nRetSp++] = ctx->lrPtr;

        SIG_WRITE("--- BACKTRACE: FP CHAIN (PC+LR only, no frame pointer) ---\n", 59);
        /* Write raw: unconditionally before maps access */
        {
            size_t ri;
            SIG_WRITE("  raw:", 6);
            for (ri = 0; ri < nRetSp; ri++)
            {
                SIG_WRITE(" ", 1);
                SigWriteHexPtr(retSp[ri]);
            }
            SIG_WRITE("\n", 1);
        }

#if LE_CONFIG_ENABLE_SEGV_HANDLER
        {
            struct sigaction sa, saveSaSegV;
            sa.sa_sigaction = (void (*)(int, siginfo_t *, void *))SigSegVHandler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_SIGINFO;
            SegvFired = 0;
            if (sigaction(SIGSEGV, &sa, &saveSaSegV))
            { SIG_WRITE("sigaction failed\n", 17); }
            if (0 == sigsetjmp(SigEnv, 1))
            {
#endif
                PrintAddrArray(retSp, (int)nRetSp, ctx->execRegions, ctx->nExecRegions);
                SIG_WRITE("\n", 1);
                SIG_WRITE("--- FP CHAIN END ---\n", 21);

                {
                    uintptr_t scanTop = (ctx->mapsTop != 0) ? ctx->mapsTop : stackTop;
                    SIG_WRITE("\n", 1);
                    SIG_WRITE("--- BACKTRACE: STACK SCAN (newest to oldest) ---\n", 49);
                    SIG_WRITE("  SP=", 5); SigWriteHexPtr((void *)scanSP);
                    SIG_WRITE(" top=", 5); SigWriteHexPtr((void *)scanTop);
                    SIG_WRITE("\n", 1);
                    if (scanSP >= 0x1000 && scanTop > scanSP)
                    {
                        ScanStackForReturnAddrs(scanSP, scanTop,
                                                 ctx->ssBase, ctx->ssTop,
                                                 ctx->execRegions, ctx->nExecRegions);
                    }
                    else
                    {
                        SIG_WRITE("  (skipped: invalid SP or top)\n", 31);
                    }
                    SIG_WRITE("--- STACK SCAN END ---\n", 23);
                }
#if LE_CONFIG_ENABLE_SEGV_HANDLER
            }
            else { SIG_WRITE("Catching SEGV while dumping the backtrace\n", 42); }
            (void)sigaction(SIGSEGV, &saveSaSegV, NULL);
        }
#endif

        /* Raw hex dump — last, skipped on altstack. */
        if (ctx->spPtr != NULL && (uintptr_t)ctx->spPtr >= 0x1000)
        {
            int onAltstack = (ctx->ssBase != 0 && ctx->ssTop != 0
                              && ((uintptr_t)ctx->spPtr < ctx->ssBase
                                  || (uintptr_t)ctx->spPtr >= ctx->ssTop));
            if (!onAltstack)
            {
                uintptr_t dumpBase = (uintptr_t)ctx->spPtr;
                uintptr_t dumpTop  = dumpBase + 512;
                uintptr_t *daddr   = (uintptr_t *)dumpBase;
                uintptr_t *dend    = (uintptr_t *)dumpTop;
                SIG_WRITE("\n--- STACK HEX (SP, 512 bytes) ---\n", 35);
                while (daddr + 8 <= dend)
                {
                    SigWriteHexPtr((void *)daddr);    SIG_WRITE(": ", 2);
                    SigWriteHexPtr((void *)daddr[0]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[1]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[2]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[3]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[4]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[5]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[6]); SIG_WRITE(" ", 1);
                    SigWriteHexPtr((void *)daddr[7]); SIG_WRITE("\n", 1);
                    daddr += 8;
                }
                SIG_WRITE("--- STACK HEX END ---\n", 22);
            }
        }
    }
}

static inline void DumpContextStack(const void *infoPtr, int skip, char *buf, size_t bufLen,
                                    pid_t crashTid)
{
    /*
     * CrashCtx_t lives in THIS frame, which has no sigsetjmp.
     * DumpRegs runs directly with no guard — it reads only mapped pages.
     * DumpBacktrace runs under a sigsetjmp guard — its longjmp unwinds back
     * to the checkpoint here, not into DumpRegs. So ctx is never clobbered.
     */
    CrashCtx_t ctx;
    (void)buf;
    (void)bufLen;
    (void)skip;

    ctx.infoPtr   = infoPtr;
    ctx.crashTid  = crashTid;
    ctx.pcPtr     = NULL;
    ctx.spPtr     = NULL;
    ctx.lrPtr     = NULL;
    ctx.fpPtr     = NULL;
    ctx.regsKnown = 0;
    ctx.ssBase          = 0;
    ctx.ssTop           = 0;
    ctx.mapsTop         = 0;
    ctx.fpChainTop      = 0;
    ctx.fpChainComplete = 0;

    /* Extract registers from ucontext if the pointer is valid */
    if (infoPtr != NULL
        && (uintptr_t)infoPtr >= 0x1000
        && ((uintptr_t)infoPtr & (sizeof(uintptr_t) - 1)) == 0)
    {
#if defined(__aarch64__)
        {
            const struct sigcontext *ctxPtr =
                (const struct sigcontext *)&(((const ucontext_t *)infoPtr)->uc_mcontext);
            const mcontext_t *mcontextPtr =
                (const mcontext_t *)&(((const ucontext_t *)infoPtr)->uc_mcontext);
            ctx.pcPtr     = (void *)ctxPtr->pc;
            ctx.spPtr     = (void *)ctxPtr->sp;
            ctx.lrPtr     = (void *)(mcontextPtr->regs[30]);
            ctx.fpPtr     = (void *)(mcontextPtr->regs[29]);
            ctx.regsKnown = 1;
        }
#elif defined(__x86_64__)
        {
            const ucontext_t *uctxPtr = (const ucontext_t *)infoPtr;
            ctx.pcPtr     = (void *)uctxPtr->uc_mcontext.gregs[REG_RIP];
            ctx.spPtr     = (void *)uctxPtr->uc_mcontext.gregs[REG_RSP];
            ctx.fpPtr     = (void *)uctxPtr->uc_mcontext.gregs[REG_RBP];
            ctx.lrPtr     = NULL;
            ctx.regsKnown = 1;
        }
#elif defined(__i586__) || defined(__i686__)
        {
            const ucontext_t *uctxPtr = (const ucontext_t *)infoPtr;
            ctx.pcPtr     = (void *)uctxPtr->uc_mcontext.gregs[REG_EIP];
            ctx.spPtr     = (void *)uctxPtr->uc_mcontext.gregs[REG_ESP];
            ctx.fpPtr     = (void *)uctxPtr->uc_mcontext.gregs[REG_EBP];
            ctx.lrPtr     = NULL;
            ctx.regsKnown = 1;
        }
#elif defined(__mips__)
        {
            const ucontext_t *uctxPtr = (const ucontext_t *)infoPtr;
            ctx.pcPtr     = (void *)uctxPtr->uc_mcontext.pc;
            ctx.spPtr     = (void *)uctxPtr->uc_mcontext.gregs[29];
            ctx.fpPtr     = (void *)uctxPtr->uc_mcontext.gregs[30];
            ctx.lrPtr     = (void *)uctxPtr->uc_mcontext.gregs[31];
            ctx.regsKnown = 1;
        }
#endif
    }

    /* Extract normal-stack bounds from uc_stack — used by DumpRegs to clamp
     * the raw dump and by DumpBacktrace to determine the scan range. */
    if (infoPtr != NULL
        && (uintptr_t)infoPtr >= 0x1000
        && ((uintptr_t)infoPtr & (sizeof(uintptr_t) - 1)) == 0)
    {
        const ucontext_t *uctx = (const ucontext_t *)infoPtr;
        uintptr_t ssBase = (uintptr_t)uctx->uc_stack.ss_sp;
        size_t    ssSize = uctx->uc_stack.ss_size;
        /* Accept only plausible alt-stack sizes:
         * MINSIGSTKSZ (2048 on aarch64) is the POSIX floor;
         * 4 MB is a generous ceiling — no real alt-stack approaches that.
         * This rejects garbage uc_stack values that would misidentify
         * a normal crash as a stack-overflow case. */
        if (ssBase >= 0x1000
            && ssSize >= MINSIGSTKSZ
            && ssSize <= 4 * 1024 * 1024)
        {
            ctx.ssBase = ssBase;
            ctx.ssTop  = ssBase + ssSize;
        }
    }


    {
        uintptr_t lookupSP = 0;
        if (ctx.spPtr != NULL && (uintptr_t)ctx.spPtr >= 0x1000)
        {
            uintptr_t sp = (uintptr_t)ctx.spPtr;
            /* Under SA_ONSTACK, uc_stack holds the ALT-STACK bounds.
             * sp in [ssBase, ssTop) → handler is running on the alt-stack
             *   → this is a stack-overflow crash; the normal stack is
             *     somewhere else — use ssBase as a probe to find it.
             * sp outside [ssBase, ssTop) → normal crash; SP is already
             *   on the normal thread stack — use it directly. */
            if (ctx.ssBase != 0 && ctx.ssTop != 0
                && sp >= ctx.ssBase && sp < ctx.ssTop)
            {
                lookupSP = ctx.ssBase;  /* stack overflow: probe via alt-stack base */
            }
            else
            {
                lookupSP = sp;          /* normal crash: SP is on the thread stack */
            }
        }
        if (lookupSP >= 0x1000)
        {
            uintptr_t mapsTop = 0;
            if (FindStackTop(lookupSP, &mapsTop) && mapsTop > lookupSP)
            {
                ctx.mapsTop = mapsTop;
            }
        }
    }

    /* Load executable regions once into ctx — shared by both phases */
    ctx.nExecRegions = LoadExecRegions(ctx.execRegions, MAX_EXEC_REGIONS);

#if LE_CONFIG_ENABLE_SEGV_HANDLER
    DumpRegs(&ctx);
    DumpBacktrace(&ctx);
#else
    DumpRegs(&ctx);
    DumpBacktrace(&ctx);
#endif
}

#endif /* end __arm__ */

void backtrace_DumpContextStack(const void *infoPtr, int skip, char *buf, size_t bufLen, pid_t tid)
{
    /* buf/bufLen are kept as parameters for API compatibility but are no longer used
     * for snprintf() — all output now goes through async-signal-safe SIG_WRITE calls. */
    (void)buf;
    (void)bufLen;
    SIG_WRITE("BACKTRACE\n", 10);

    DumpContextStack(infoPtr, skip, buf, bufLen, tid);

    SIG_WRITE("DONE\n", 5);
}

#if LE_CONFIG_ENABLE_BACKTRACE

void _le_backtrace(const char *msg)
{
    char        buffer[256];
    ucontext_t  ctx;

    if (getcontext(&ctx) < 0)
    {
        // Do not use LE_ERRNO_TXT() here, as that uses TLS which may not be available
        // in a crash situation.
#if (_POSIX_C_SOURCE >= 200112L) && !  _GNU_SOURCE
        // XSI-compliant strerror_r
        if (strerror_r(errno, buffer, sizeof(buffer)) == 0)
        {
            // OK to return this since it's a static thread-local buffer.
            msg = buffer;
        }
        else
        {
            msg = "Unknown error";
        }
#else
        // GNU strerror_r
        msg = strerror_r(errno, buffer, sizeof(buffer));
#endif
        LE_ERROR("Failed to get context for backtrace: (%d) %s", errno, msg);
        return;
    }

    fprintf(stderr, "%s\n", msg);
    backtrace_DumpContextStack(&ctx, 1, buffer, sizeof(buffer), (pid_t)syscall(SYS_gettid));
}

#endif /* end LE_CONFIG_ENABLE_BACKTRACE */
