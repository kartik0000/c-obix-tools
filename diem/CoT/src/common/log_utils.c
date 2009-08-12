/* *****************************************************************************
 * Copyright (c) 2009 Andrey Litvinov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/
/** @file
 * Contains implementation of logging tools.
 *
 * @author Andrey Litvinov
 * @version 2.0
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "bool.h"
#include "log_utils.h"

/** @name Logging to @a stdout
 * @{ */
static void log_debugPrintf(char* fmt, ...);
static void log_warningPrintf(char* fmt, ...);
static void log_errorPrintf(char* fmt, ...);
/** @}
 * @name Logging using @a syslog
 * @{ */
static void log_debugSyslog(char* fmt, ...);
static void log_warningSyslog(char* fmt, ...);
static void log_errorSyslog(char* fmt, ...);
/** @} */

/** @name Log handlers.
 * @{ */
log_function log_debugHandler = &log_debugPrintf;
log_function log_warningHandler = &log_warningPrintf;
log_function log_errorHandler = &log_errorPrintf;
/** @} */

static int _log_level = LOG_LEVEL_DEBUG;

static BOOL _use_syslog = FALSE;

static void log_debugPrintf(char* fmt, ...)
{
    printf("DEBUG ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

static void log_warningPrintf(char* fmt, ...)
{
    printf("WARNING ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

static void log_errorPrintf(char* fmt, ...)
{
    printf("ERROR ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

static void log_debugSyslog(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsyslog(LOG_DEBUG, fmt, args);
    va_end(args);
}

static void log_warningSyslog(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsyslog(LOG_WARNING, fmt, args);
    va_end(args);
}

static void log_errorSyslog(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsyslog(LOG_ERR, fmt, args);
    va_end(args);
}

static void log_nothing(char* fmt, ...)
{
    // dismiss the log message
}

static void setPrintf()
{
    // drop all log functions
    log_debugHandler = &log_nothing;
    log_warningHandler = &log_nothing;
    log_errorHandler = &log_nothing;
    // set corresponding log functions
    switch(_log_level)
    {
    case LOG_LEVEL_DEBUG:
        log_debugHandler = &log_debugPrintf;
    case LOG_LEVEL_WARNING:
        log_warningHandler = &log_warningPrintf;
    case LOG_LEVEL_ERROR:
        log_errorHandler = &log_errorPrintf;
    default:
    	break;
    }
}

static void setSyslog()
{
    // drop all log functions
    log_debugHandler = &log_nothing;
    log_warningHandler = &log_nothing;
    log_errorHandler = &log_nothing;
    // set corresponding log functions
    switch(_log_level)
    {
    case LOG_LEVEL_DEBUG:
        log_debugHandler = &log_debugSyslog;
    case LOG_LEVEL_WARNING:
        log_warningHandler = &log_warningSyslog;
    case LOG_LEVEL_ERROR:
        log_errorHandler = &log_errorSyslog;
    default:
    	break;
    }
}

void log_usePrintf()
{
    _use_syslog = FALSE;
    // close syslog connection if it was opened
    closelog();
    setPrintf();
}



void log_useSyslog(int facility)
{
    _use_syslog = TRUE;
    openlog(NULL, LOG_NDELAY, facility);
    setSyslog();
}

void log_setLevel(LOG_LEVEL level)
{
    _log_level = level;
    if (_use_syslog)
    {
        setSyslog();
    }
    else
    {
        setPrintf();
    }
}
