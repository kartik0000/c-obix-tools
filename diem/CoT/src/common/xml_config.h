#ifndef XML_CONFIG_H_
#define XML_CONFIG_H_
/** @file
 * @brief Declares configuration API.
 *
 * Configuration API allows loading settings from XML file which contains
 * \<config> element. This header defines required functions which simplify
 * parsing XML settings.
 *
 * The API is tightly integrated with the logging system (lwl_ext.h). Log
 * settings are loaded automatically from a parsed configuration file and all
 * functions extensively use lwl_ext.h utilities for logging warning and error
 * messages occurred during parsing. @b Note that errors which occur during
 * initial file reading (before log settings are loaded) are handled by
 * uninitialized logging system (i.e. messages are forwarded to standard
 * output).
 *
 * @author Andrey Litvinov
 * @version 1.1
 */

#include <ixml_ext.h>

/** Main configuration tag's name (CT - Config Tag) */
extern const char* CT_CONFIG;
/** Most commonly used tag attribute: 'val' (CTA - Config Tag Attribute) */
extern const char* CTA_VALUE;

/**
 * Sets the address of the resource folder where configuration file is stored.
 * The general idea is to keep all resource files in one place which is defined
 * only once in the application. After that all resources (including configuration
 * file) can be reached using #config_getResFullPath().
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
 * - Searches for \<config> tag;
 * - Loads log settings from \<log> tag (see #log_config()).
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
 * Releases resources allocated for settings parsing.
 * Should be called once after all settings are loaded (or failed to load).
 * Also writes message to log, telling that initialization is completed.
 * Depending on @a successful parameter the log message tells that
 * initialization completed or failed.
 *
 * @param successful Tells whether application initialized successfully or not.
 */
void config_finishInit(BOOL successful);

/**
 * Releases all allocated resources and cleans loaded log settings.
 * All settings are dropped including log system configuration (does the same as
 * #log_dispose()). Should be called during application shutdown.
 */
void config_dispose();

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
