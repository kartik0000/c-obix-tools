#ifndef XML_CONFIG_H_
#define XML_CONFIG_H_
/** @file
 * @brief Declares configuration API.
 *
 * Configuration API allows loading settings from XML file which contains
 * <config> element. This header defines required functions which simplify
 * parsing XML settings.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <ixml_ext.h>


/**@name Config file parameter names
 * See more about parameters at the #OBIX_CONFIG_FILE.
 */
/*@{*/
///@brief Main tag name (CT - Config Tag)
extern const char* CT_CONFIG;
///@brief Tag attribute 'value' (CTA - Config Tag Attribute)
extern const char* CTA_VALUE;
/*@}*/

/**
 * @brief Opens config file and checks its structure
 * @return pointer to the config xml node.
 */
IXML_Element* config_loadFile(const char* filename);

IXML_Element* config_getChildTag(IXML_Element* conf, const char* tagName, BOOL obligatory);

const char* config_getChildTagValue(IXML_Element* conf, const char* tagName, BOOL obligatory);

const char* config_getTagAttributeValue(IXML_Element* tag, const char* attrName, BOOL obligatory);

/**
 *
 * @return @li @a >=0 - Parsed integer attribute value;
 *         @li @a -1  - Error code.
 */
int config_getTagIntAttrValue(IXML_Element* tag, const char* attrName, BOOL obligatory, int defaultValue);

/**
 *
 * @return @li @a >=0 - Parsed long integer attribute value;
 *         @li @a -1  - Error code.
 */
long config_getTagLongAttrValue(IXML_Element* tag, const char* attrName, BOOL obligatory, long defaultValue);

/**
 * @return @li @a 0 - False;
 *         @li @a 1 - True;
 *         @li @a -1 - Error.
 */
int config_getTagBoolAttrValue(IXML_Element* tag, const char* attrName, BOOL obligatory);

void config_finishInit(BOOL successful);

void config_dispose();

/**
 * Returns the address of the resource file by adding required path to the
 * filename. <b>Note:</b> Do not forget to release memory after using.
 *
 * @return address to the resource file.
 */
char* config_getResFullPath(const char* filename);

void config_setResourceDir(char* path);

#endif /*XML_CONFIG_H_*/
