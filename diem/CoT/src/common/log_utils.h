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
 * Definitions of logging tools.
 *
 * Log system has two modes:
 * - @a syslog - when all log messages are forwarded to syslog;
 * - @a printf - all messages are printed using @a printf utility, thus in most
 * cases are shown on a console.
 *
 * The default mode is @a printf, but it can be switched at any time.
 *
 * Library provides three simple methods for logging messages with different
 * priority levels:
 * - #log_debug()
 * - #log_warning()
 * - #log_error()
 *
 * @author Andrey Litvinov
 * @version 2.0
 */

#ifndef LOG_UTILS_H_
#define LOG_UTILS_H_

/**
 * This is a prototype of log handler function.
 * @param fmt Message format (used in the same way as with @a printf()).
 */
typedef void (*log_function)(char* fmt, ...);

/**@name Log handlers
 * Contain links to the current log handlers. Normally these links should not
 * be used directly.
 * @{
 */
/**
 * Contains link to the current handler of @a debug log messages. Normally
 * #log_debug() should be used instead.
 */
extern log_function log_debugHandler;
/**
 * Contains link to the current handler of @a warning log messages. Normally
 * #log_warning() should be used instead.
 */
extern log_function log_warningHandler;
/**
 * Contains link to the current handler of @a error log messages. Normally
 * #log_error() should be used instead.
 */
extern log_function log_errorHandler;
/** @} */

/**@name Logging utilities
 * @{*/
// A trick with define is done in order to auto-add filename and line number
// into the log message.

/**
 * Prints @a debug message to the configured output.
 * Automatically adds filename and string number of the place from where the log
 * was written.
 *
 * @param fmt Message format (used in the same way as with @a printf()).
 */
#define log_debug(fmt, ...) (*log_debugHandler)("%s(%d): " fmt, __FILE__, \
 __LINE__, ## __VA_ARGS__)

/**
 * Prints @a warning message to the configured output.
 * Automatically adds filename and string number of the place from where the log
 * was written.
 *
 * @param fmt Message format (used in the same way as with @a printf()).
 */
#define log_warning(fmt, ...) (*log_warningHandler)("%s(%d): " fmt, __FILE__, \
 __LINE__, ## __VA_ARGS__)

/**
 * Prints @a error message to the configured output.
 * Automatically adds filename and string number of the place from where the log
 * was written.
 *
 * @param fmt Message format (used in the same way as with @a printf()).
 */
#define log_error(fmt, ...) (*log_errorHandler)("%s(%d): " fmt, __FILE__, \
 __LINE__, ## __VA_ARGS__)
/**@}*/

/**
 * Defines possible log levels.
 */
typedef enum
{
	/** Debug log level. */
	LOG_LEVEL_DEBUG,
	/** Warning log level. */
	LOG_LEVEL_WARNING,
	/** Error log level. */
	LOG_LEVEL_ERROR,
	/** 'No' log level. */
	LOG_LEVEL_NO
} LOG_LEVEL;

/**
 * Switches library to use @a syslog for handling messages.
 *
 * @param facility Facility tells syslog who issued the message. See
 * documentation of @a syslog for more information.
 */
void log_useSyslog(int facility);

/**
 * Switches library to use @a printf for handling messages.
 */
void log_usePrintf();

/**
 * Sets the minimum priority level of the messages which will be processed.
 *
 * @param level Priority level:
 *              - #LOG_LEVEL_DEBUG - All messages will be printed;
 *              - #LOG_LEVEL_WARNING - Only warning and error messages will be
 *                                     printed;
 *              - #LOG_LEVEL_ERROR - Only error messages are printed;
 *              - #LOG_LEVEL_NO - Nothing is printed at all.
 */
void log_setLevel(LOG_LEVEL level);

#endif /* LOG_UTILS_H_ */
