//--------------------------------------------------------------------------------------------------
/** @file signals.h
 *
 * This module contains initialization functions for the Legato signal events system that should
 * be called by the build system.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#ifndef LEGATO_SIG_INCLUDE_GUARD
#define LEGATO_SIG_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * WRITE macro to discard the return code inside the ShowStackSignalHandler
 */
//--------------------------------------------------------------------------------------------------
#define SIG_WRITE(buffer, sz)                                           \
    do {                                                                \
        const char *_buf = (const char *)(buffer);                      \
        ssize_t _total = (ssize_t)(sz);                                 \
        ssize_t _written = 0;                                           \
        ssize_t _rc;                                                    \
        while (_written < _total)                                       \
        {                                                               \
            _rc = write(STDERR_FILENO, _buf + _written,                 \
                        (size_t)(_total - _written));                   \
            if (_rc <= 0) { break; }                                    \
            _written += _rc;                                            \
        }                                                               \
    } while(0)

//--------------------------------------------------------------------------------------------------
/**
 * Async-signal-safe output helpers.
 *
 * These are declared static inline here so that both signals.c and backtrace.c (which
 * #include this header) each get their own copy without any external linkage requirement.
 * All three functions use only stack-local buffers and write(2), which is on the POSIX
 * async-signal-safe list. No malloc, no locale, no locks.
 *
 * SigWriteStr    - write a NUL-terminated literal string to STDERR_FILENO.
 * SigWriteULong  - write an unsigned long as decimal digits to STDERR_FILENO.
 * SigWriteHexPtr - write a pointer as "0x<hex>" to STDERR_FILENO (pointer-width padded).
 * SigWriteHex32  - write a uint32_t as exactly 8 hex digits to STDERR_FILENO.
 * SigWriteHexOffset - write a uintptr_t as "0x<hex>" with no leading zeros (for library offsets).
 */
//--------------------------------------------------------------------------------------------------
static inline void SigWriteStr(const char *str)
{
    if (str == NULL) { return; }
    /* strlen() is async-signal-safe (POSIX.1-2008 TC2). */
    SIG_WRITE(str, strlen(str));
}

static inline void SigWriteULong(unsigned long val)
{
    char buf[21]; /* 20 decimal digits + NUL for a 64-bit value */
    int  i = (int)(sizeof(buf)) - 1;
    buf[i] = '\0';
    if (val == 0)
    {
        buf[--i] = '0';
    }
    else
    {
        while (val > 0)
        {
            buf[--i] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    SIG_WRITE(buf + i, (size_t)((int)(sizeof(buf)) - 1 - i));
}

static inline void SigWriteHexPtr(const void *ptr)
{
    static const char _hex[] = "0123456789abcdef";
    char buf[19]; /* "0x" + 16 hex digits + NUL */
    uintptr_t val = (uintptr_t)ptr;
    int i = (int)(sizeof(buf)) - 1;
    buf[i] = '\0';
    do
    {
        buf[--i] = _hex[val & 0xF];
        val >>= 4;
    }
    while (val != 0);
    /* Pad to full pointer width so addresses align in the output. */
    while (i > 2)
    {
        buf[--i] = '0';
    }
    buf[--i] = 'x';
    buf[--i] = '0';
    SIG_WRITE(buf + i, (size_t)((int)(sizeof(buf)) - 1 - i));
}

static inline void SigWriteHex32(uint32_t val)
{
    static const char _hex[] = "0123456789abcdef";
    char buf[9]; /* exactly 8 hex digits + NUL */
    int i;
    for (i = 7; i >= 0; i--)
    {
        buf[i] = _hex[val & 0xF];
        val >>= 4;
    }
    buf[8] = '\0';
    SIG_WRITE(buf, 8);
}

static inline void SigWriteHexOffset(uintptr_t val)
{
    /*
     * Write a uintptr_t as "0x" followed by hex digits with NO leading zeros.
     * Used for library-relative offsets in PrintOneAddr so the output matches
     * the compact backtrace_symbols_fd style:
     *   /lib/libc.so.6(+0xf7020)[0x7fb0987020]
     * rather than the full-width pointer style:
     *   /lib/libc.so.6(+0x000000000f7020)[0x7fb0987020]
     */
    static const char _hex[] = "0123456789abcdef";
    char buf[19]; /* "0x" + up to 16 hex digits + NUL */
    int i = (int)(sizeof(buf)) - 1;
    buf[i] = '\0';
    if (val == 0)
    {
        buf[--i] = '0';
    }
    else
    {
        while (val != 0)
        {
            buf[--i] = _hex[val & 0xF];
            val >>= 4;
        }
    }
    buf[--i] = 'x';
    buf[--i] = '0';
    SIG_WRITE(buf + i, (size_t)((int)(sizeof(buf)) - 1 - i));
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
);


#endif  // LEGATO_SIG_INCLUDE_GUARD
