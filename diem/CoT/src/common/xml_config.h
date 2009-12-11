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
#ifndef XML_CONFIG_H_
#define XML_CONFIG_H_
/** @file
 * @brief Declares configuration API.
 *
 * Configuration API allows loading settings from XML file which contains
 * \<config> element. This header defines required functions which simplify
 * parsing of XML settings.
 *
 * The API is tightly integrated with the logging system (log_utils.h). All
 * functions extensively use log_utils.h utilities for logging warning and error
 * messages occurred during parsing. There is also a possibility to load log
 * system configuration from an XML file.
 *
 * @author Andrey Litvinov
 */

#include <ixml_ext.h>

/**@name Common configuration file keywords
 * The following acronyms are used in constants' names:
 * - @a CT means Configuration Tag;
 * - @a CTA - Configuration Tag's Attribute;
 * - @a CTAV - Configuration Tag's Attribute's Value.
 * @{
 */
/** Main configuration tag's name. */
extern const char* CT_CONFIG;
/** Most commonly used tag attribute: 'val' */
extern const char* CTA_VALUE;
/** @} */

/**@name Log configuration keywords
 * These variables contain names of all log configuration tags, their attributes
 * and possible attributes' values.
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
/** Defines whether @a syslog should be used for logging. If tag with such name
 * is not present, than all messages are printed to @a stdout. */
extern const char* CT_LOG_USE_SYSLOG;
/** Attribute which defines syslog facility. */
extern const char* CTA_LOG_FACILITY;
/** Log facility 'user'. */
extern const char* CTAV_LOG_FACILITY_USER;
/** Log facility 'daemon'. */
extern const char* CTAV_LOG_FACILITY_DAEMON;
/** Log facility 'local0'. */
extern const char* CTAV_LOG_FACILITY_LOCAL0;
/** @} */

/**
 * Sets the address of the resource folder where configuration file is stored.
 * The general idea is to keep all resource files in one place which is defined
 * only once in the application. After that all resources (including
 * configuration file) can be reached using #config_getResFullPath().
 *
 * @param path Path to the application's resource folder.
 */
void config_setResourceDir(char* path);

/**
 * Returns the address of the resource file by adding resource folder path to
 * the filename.
 * @note Do not forget to release memory allocated for returned string.
 *
 * @param filename Name of the file.
 * @return Full path to the resource file, or @a NULL on error.
 */
char* config_getResFullPath(const char* filename);

/**
 * Opens the configuration file and performs it's initial parsing.
 *
 * @note Returned XML DOM structure should not be freed manually. Instead, use
 *       #config_finishInit() after parsing all required settings.
 *
 * @param filename Name of the configuration file.
 * @return Reference to the \<config> tag, which can be used to load application
 *         settings.
 */
IXML_Element* config_loadFile(const char* filename);

/**
 * Configures log system using parameters passed in XML format.
 * Passed XML should contain at least @a \<log> tag with child @a \<level>,
 * which specifies log level. For example:
 * @code
 * <log>
 *  <level val="debug" />
 * </log>
 * @endcode
 *
 * There are other optional configuration tags. Please refer to the @a \<log>
 * element in example_timer_config.xml for the full description of possible
 * tags.
 *
 * @param configTag XML configuration document, which contains log configuration
 *                  tags. Document can contain other elements which are ignored.
 * @return @a 0 on success; @a -1 on error.
 */
int config_log(IXML_Element* configTag);

/**
 * Releases resources allocated for settings parsing.
 * Should be called once after all settings are loaded (or failed to load).
 * Also writes message to log, telling that initialization is completed.
 * Depending on @a successful parameter the log message tells that
 * initialization completed or failed.
 *
 * @param conf Settings which should be freed. It can be the link returned by
 *             #config_loadFile() or a link to any child tag.
 * @param successful Tells whether application initialized successfully or not.
 */
void config_finishInit(IXML_Element* conf, BOOL successful);

/**
 * Returns child tag of the provided element with the specified name.
 *
 * @param conf Configuration tag in which search should be performed.
 * @param tagName Name of the tag to be searched for.
 * @param obligatory If #TRUE than an error message will be logged when the tag
 *                   is not found.
 * @return Found child tag, or @a NULL if nothing is found.
 */
IXML_Element* config_getChildTag(IXML_Element* conf,
                                 const char* tagName,
                                 BOOL obligatory);

/**
 * Returns value of the child tag with specified name.
 *
 * Tag value is the value of it's @ref CTA_VALUE "val" attribute.
 *
 * @param conf Configuration tag in which search should be performed.
 * @param tagName Name of the tag to be searched for.
 * @param obligatory If #TRUE than an error message will be logged when the tag
 *                   is not found, or it doesn't have any value.
 * @return Value of the found child tag or @a NULL if the tag is not found or
 *         the found one doesn't have any value.
 */
const char* config_getChildTagValue(IXML_Element* conf,
                                    const char* tagName,
                                    BOOL obligatory);

/**
 * Returns value of the specified tag attribute.
 *
 * @param tag Configuration tag whose attribute should be returned.
 * @param attrName Name of the attribute to read.
 * @param obligatory if #TRUE than an error message will be written when the
 *                   attribute is not found.
 * @return Attribute's value or @a NULL if the attribute is not found.
 */
const char* config_getTagAttributeValue(IXML_Element* tag,
                                        const char* attrName,
                                        BOOL obligatory);

/**
 * Returns integer value of the specified tag attribute. Can be used only for
 * parsing positive values.
 *
 * @param tag Configuration tag whose attribute should be parsed.
 * @param attrName Name of the attribute to read.
 * @param obligatory if #TRUE than an error message will be written when the
 *                   attribute is not found.
 * @param defaultValue if @a obligatory is #FALSE than this value will be
 *                     returned instead of the error code.
 * @return @li @a >=0 - Parsed integer attribute value;
 *         @li @a -1  - Error code. Error description will be logged.
 */
int config_getTagAttrIntValue(IXML_Element* tag,
                              const char* attrName,
                              BOOL obligatory,
                              int defaultValue);

/**
 * Returns long integer value of the specified tag attribute. Can be used only
 * for parsing positive values.
 *
 * @param tag Configuration tag whose attribute should be parsed.
 * @param attrName Name of the attribute to read.
 * @param obligatory if #TRUE than an error message will be written when the
 *                   attribute is not found.
 * @param defaultValue if @a obligatory is #FALSE than this value will be
 *                     returned instead of the error code.
 * @return @li @a >=0 - Parsed long integer attribute value;
 *         @li @a -1  - Error code. Error description will be logged.
 */
long config_getTagAttrLongValue(IXML_Element* tag,
                                const char* attrName,
                                BOOL obligatory,
                                long defaultValue);

/**
 * Returns boolean value of the specified tag attribute.
 *
 * @param tag Configuration tag whose attribute should be parsed.
 * @param attrName Name of the attribute to read.
 * @param obligatory if #TRUE than an error message will be written when the
 *                   attribute is not found.
 * @return @li @a 0 - Attribute's value is @a "false";
 *         @li @a 1 - Attribute's value is @a "true";
 *         @li @a -1 - Parsing error.
 */
int config_getTagAttrBoolValue(IXML_Element* tag,
                               const char* attrName,
                               BOOL obligatory);

#endif /*XML_CONFIG_H_*/
