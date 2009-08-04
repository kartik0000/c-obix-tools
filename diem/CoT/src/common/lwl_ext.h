/** @file
 * Definitions of logging tools.
 *
 * Log system is based on @a liblwl (Log Writer Library
 * http://gna.org/projects/lwl/).
 * It provides three simple methods for logging messages with different
 * priorities:
 * - #log_debug()
 * - #log_warning()
 * - #log_error()
 *
 * Before using them, log system should be configured by calling #log_config().
 * Otherwise all messages will be logged to the standard output (@a stdout).
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef LWL_EXT_H_
#define LWL_EXT_H_

#include "ixml_ext.h"

/**@name Log configuration names
 * These variables contain names of all log configuration tags, their attributes
 * and possible attributes' values. The following acronyms are used in the
 * variable names:
 * - @a CT means Configuration Tag;
 * - @a CTA - Configuration Tag's Attribute;
 * - @a CTAV - Configuration Tag's Attribute's Value.
 * @{
 */
/** Parent log configuration tag, containing all settings. */
extern const char* CT_LOG;
/** Log level tag. Defines which messages are logged. */
extern const char* CT_LOG_LEVEL;
/** Log level @a debug. All messages are logged. */
extern const char* CTAV_LOG_LEVEL_DEBUG;
/** Log level @a warning. Only @a warning and @a error messages are logged. */
extern const char* CTAV_LOG_LEVEL_WARNING;
/** Log level @a error. Only @a error messages are logged. */
extern const char* CTAV_LOG_LEVEL_ERROR;
/** Log level @a no. Nothing is logged at all. */
extern const char* CTAV_LOG_LEVEL_NO;
/** Tag containing log file name. */
extern const char* CT_LOG_FILE;
/** Tag which configures format of the logged messages. */
extern const char* CT_LOG_FORMAT;
/** Defines text prefix which will be added to every log message. */
extern const char* CTA_LOG_PREFIX;
/** Defines whether date should be added to each message. */
extern const char* CTA_LOG_DATE;
/** Defines whether time should be added to each message. */
extern const char* CTA_LOG_TIME;
/** Defines whether date and time should be written according to the local
 * settings. */
extern const char* CTA_LOG_LOCALE;
/** Defines whether priority of the message should be added to each message. */
extern const char* CTA_LOG_PRIORITY;
/** If presents than each message will not be forced to be written to the log
 * file immediately. This will allow operating system to buffer messages, which
 * from the one hand will increase performance, but from the other, can delay
 * appearing of the log messages in the file. */
extern const char* CT_LOG_NO_FLUSH;
/** @} */

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
 * Configures log system using parameters passed in XML format.
 * Passed XML should contain at least @a <log> tag with child @a <level>, which
 * specifies log level. For example:
 * @code
 * <log>
 *  <level val="debug" />
 * </log>
 * @endcode
 *
 * There are other optional configuration tags. Please refer to the <log>
 * element in example_timer_config.xml for the full description of possible
 * tags.
 *
 * @param configTag XML configuration document, which contains log configuration
 *                  tags. Document can contain other elements which are ignored.
 * @return @a 0 on success; @a -1 on error.
 */
int log_config(IXML_Element* configTag);

/**
 * Clears memory allocated for the logging utility. Should be called in the very
 * end of the program execution. Also resets all log settings, thus all messages
 * logged after calling this method will be written to the standard output
 * (stdout).
 */
void log_dispose();

#endif /* LWL_EXT_H_ */
