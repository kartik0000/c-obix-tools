/** @file
 * Contains definitions of logging tools.
 *
 * Log system is based on @a liblwl library.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef LWL_EXT_H_
#define LWL_EXT_H_

#include "ixml_ext.h"

extern const char* CT_LOG;
extern const char* CT_LOG_LEVEL;
extern const char* CTAV_LOG_LEVEL_DEBUG;
extern const char* CTAV_LOG_LEVEL_WARNING;
extern const char* CTAV_LOG_LEVEL_ERROR;
extern const char* CTAV_LOG_LEVEL_NO;
extern const char* CT_LOG_FILE;
extern const char* CT_LOG_FORMAT;
extern const char* CTA_LOG_PREFIX;
extern const char* CTA_LOG_DATE;
extern const char* CTA_LOG_TIME;
extern const char* CTA_LOG_LOCALE;
extern const char* CTA_LOG_PRIORITY;
extern const char* CT_LOG_NO_FLUSH;

typedef void (*log_function)(char* fmt, ...);

extern log_function log_debugHandler;
extern log_function log_warningHandler;
extern log_function log_errorHandler;

/**@name Logging utilities@{*/
// A trick with define is done in order to auto-add filename and line number
// into the log message.
#define log_debug(fmt, ...) (*log_debugHandler)("%s(%d): " fmt, __FILE__, \
												__LINE__, ## __VA_ARGS__)

#define log_warning(fmt, ...) (*log_warningHandler)("%s(%d): " fmt, __FILE__, \
													__LINE__, ## __VA_ARGS__)

#define log_error(fmt, ...) (*log_errorHandler)("%s(%d): " fmt, __FILE__, \
												__LINE__, ## __VA_ARGS__)
/**@}*/

int log_config(IXML_Element* configTag);

void log_dispose();

#endif /* LWL_EXT_H_ */
