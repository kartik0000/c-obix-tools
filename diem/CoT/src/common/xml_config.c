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
 * @todo add description
 *
 * @author Andrey Litvinov
 * @version 1.1
 */
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "xml_config.h"
#include "log_utils.h"
#include "obix_utils.h"

const char* CT_CONFIG = "config";
const char* CTA_VALUE = "val";

const char* CT_LOG = "log";
const char* CT_LOG_LEVEL = "level";
const char* CTAV_LOG_LEVEL_DEBUG = "debug";
const char* CTAV_LOG_LEVEL_WARNING = "warning";
const char* CTAV_LOG_LEVEL_ERROR = "error";
const char* CTAV_LOG_LEVEL_NO = "no";
const char* CT_LOG_USE_SYSLOG = "use-syslog";
const char* CTA_LOG_FACILITY = "facility";
const char* CTAV_LOG_FACILITY_USER = "user";
const char* CTAV_LOG_FACILITY_DAEMON = "daemon";
const char* CTAV_LOG_FACILITY_LOCAL0 = "local0";

static char* resourceFolder;

IXML_Element* config_getChildTag(IXML_Element* conf,
                                 const char* tagName,
                                 BOOL obligatory)
{
    IXML_NodeList* list = ixmlElement_getElementsByTagName(conf, tagName);
    if (list == NULL)
    {
        if (obligatory == TRUE)
        {
            log_error("Obligatory configuration tag <%s> is not found.",
                      tagName);
        }
        return NULL;
    }

    IXML_Element* element = ixmlNode_convertToElement(
                                ixmlNodeList_item(list, 0));
    ixmlNodeList_free(list);

    if (element == NULL)
    {
        if (obligatory == TRUE)
        {
            log_error("Obligatory configuration tag <%s> is not found.",
                      tagName);
        }
        log_error("Internal error! This should never happen.");
        return NULL;
    }

    return (IXML_Element*) element;
}

const char* config_getChildTagValue(IXML_Element* conf,
                                    const char* tagName,
                                    BOOL obligatory)
{
    IXML_Element* element = config_getChildTag(conf, tagName, obligatory);
    if (element == NULL)
    {	// error is already logged
        return NULL;
    }

    return config_getTagAttributeValue(element, CTA_VALUE, obligatory);
}

const char* config_getTagAttributeValue(IXML_Element* tag, const char* attrName, BOOL obligatory)
{
    const char* val = ixmlElement_getAttribute(tag, attrName);
    if ((val == NULL) && (obligatory == TRUE))
    {
        const char* tagName = ixmlElement_getTagName(tag);
        log_error("Obligatory attribute \"%s\" of configuration tag <%s> is not found.", attrName, tagName);
    }
    return val;
}

int config_getTagAttrIntValue(IXML_Element* tag, const char* attrName, BOOL obligatory, int defaultValue)
{
    long val = config_getTagAttrLongValue(tag, attrName, obligatory, defaultValue);
    if (val < 0)
    {	// error is already logged
        return val;
    }

    if (val > INT_MAX)
    {
        // number is too big
        if (obligatory)
        {
            log_error("The value of obligatory attribute \"%s\" of "
                      "configuration tag <%s> is too big (%ld).",
                      attrName, ixmlElement_getTagName(tag), val);
            return -1;
        }
        else
        {
            log_error("The value of attribute \"%s\" of configuration tag <%s> "
                      "is too big (%ld). Using %d by default.",
                      attrName, ixmlElement_getTagName(tag),
                      val, defaultValue);
            return defaultValue;
        }
    }

    return (int) val;
}

long config_getTagAttrLongValue(IXML_Element* tag, const char* attrName, BOOL obligatory, long defaultValue)
{
    const char* attrValue = config_getTagAttributeValue(tag, attrName, obligatory);
    if (attrValue == NULL)
    {
        if (obligatory)
        {
            // error is already logged
            return -1;
        }
        else
        {
            log_debug("Optional attribute \"%s\" of configuration tag <%s> is "
                      "not found. Using %ld by default.",
                      attrName, ixmlElement_getTagName(tag), defaultValue);
            return defaultValue;
        }
    }

    // try to parse integer value
    char* endPtr;
    long val = strtol(attrValue, &endPtr, 10);

    if ((endPtr == attrValue) || (val < 0))
    {
        // conversion failed
        if (obligatory)
        {
            log_error("Obligatory attribute \"%s\" of configuration tag <%s> is not"
                      " a positive integer (%s).",
                      attrName, ixmlElement_getTagName(tag), attrValue);
            return -1;
        }
        else
        {
            log_error("Attribute \"%s\" of configuration tag <%s> is not a "
                      "positive integer (%s). Using %ld by default.",
                      attrName, ixmlElement_getTagName(tag),
                      attrValue, defaultValue);
            return defaultValue;
        }
    }

    return val;
}

int config_getTagAttrBoolValue(IXML_Element* tag, const char* attrName, BOOL obligatory)
{
    const char* attrValue = config_getTagAttributeValue(tag, attrName, obligatory);
    // TODO: code assumes that TRUE == 1 and FALSE == 0
    if (attrValue == NULL)
    {
        if (obligatory)
        {
            return -1;
        }
        else
        {
            log_debug("Optional attribute \"%s\" of tag <%s> is not found. "
                      "Setting \"false\" by default.",
                      attrName, ixmlElement_getTagName(tag));
            return FALSE;
        }
    }

    if (!strcmp(attrValue, XML_TRUE))
    {
        return TRUE;
    }
    else if (!strcmp(attrValue, XML_FALSE))
    {
        return FALSE;
    }
    else
    {
        const char* tagName = ixmlElement_getTagName(tag);
        if (obligatory)
        {
            log_error("Attribute \"%s\" of tag <%s> has wrong value. "
                      "Possible values: \"true\" or \"false\".", attrName, tagName);
            return -1;
        }
        else
        {
            log_warning("Attribute \"%s\" of tag <%s> has wrong value. "
                        "Possible values: \"true\" or \"false\". "
                        "Setting \"false\" by default.", attrName, tagName);
            return FALSE;
        }
    }
}

IXML_Element* config_loadFile(const char* filename)
{
    char* path = config_getResFullPath(filename);

    IXML_Document* xmlConfigDoc;
    //Trying to load the configuration file
    int error = ixmlLoadDocumentEx(path, &xmlConfigDoc);

    if (error != IXML_SUCCESS)
    {
        switch(error)
        {
        case IXML_NO_SUCH_FILE:
            log_error("Error reading the configuration file \'%s\': "
                      "File is not found.", path);
            break;
        case IXML_SYNTAX_ERR:
        case IXML_FAILED:
            log_error("Error reading the configuration file \'%s\': "
                      "XML syntax error.", path);
            break;
        default:
            log_error("Error reading the configuration file \'%s\': "
                      "ixmlLoadDocumentEx returned %d.", path, error);
            break;
        }

        free(path);
        return NULL;
    }
    free(path);

    //get the root configure tag from the document
    IXML_Element* configTag = config_getChildTag(
                                  ixmlDocument_getRootElement(xmlConfigDoc),
                                  CT_CONFIG,
                                  TRUE);

    return configTag;
}

void config_finishInit(IXML_Element* conf, BOOL successful)
{
    ixmlElement_freeOwnerDocument(conf);
    if (successful)
    {
        log_debug("!!!!   Initialization completed successfully   !!!!");
    }
    else
    {
        log_error("!!!!           Initialization failed           !!!!");
    }
}

char* config_getResFullPath(const char* filename)
{
    if (resourceFolder == NULL)
    {	// resource folder is not set, so there is nothing to add
        return strdup(filename);
    }

    // concatenate filename with resource folder
    char* output = (char*) malloc(strlen(filename) +
                                  strlen(resourceFolder) + 1);
    if (output == NULL)
    {
        log_error("Not enough memory.");
        return NULL;
    }
    strcpy(output, resourceFolder);
    strcat(output, filename);

    return output;
}

void config_setResourceDir(char* path)
{
    if (resourceFolder != NULL)
    {
        free(resourceFolder);
    }

    int length = strlen(path);
    if (path[length - 1] == '/')
    {
        resourceFolder = strdup(path);
    }
    else
    {
        // add trailing slash to the address
        resourceFolder = (char*) malloc(length + 2);
        strcpy(resourceFolder, path);
        resourceFolder[length] = '/';
        resourceFolder[length + 1] = '\0';
    }

    if (resourceFolder == NULL)
    {
        log_error("Unable to set resource folder path. "
                  "Input argument was: \"%s\".\n"
                  "Using current folder instead.", path);
    }
    else
    {
        log_debug("Resource folder path is set to \"%s\".", resourceFolder);
    }
}

int config_log(IXML_Element* configTag)
{
    IXML_Element* logTag = config_getChildTag(configTag, CT_LOG, TRUE);
    if (logTag == NULL)
        return -1;

    //get the log level.
    int logLevel;
    IXML_Element* tempTag = config_getChildTag(logTag, CT_LOG_LEVEL, TRUE);
    if (tempTag == NULL)
        return -1;

    const char* tempStr = config_getTagAttributeValue(tempTag, CTA_VALUE, TRUE);
    if (tempStr == NULL)
        return -1;

    if (!strcmp(tempStr, CTAV_LOG_LEVEL_DEBUG))
    {
        logLevel = LOG_LEVEL_DEBUG;
    }
    else if (!strcmp(tempStr, CTAV_LOG_LEVEL_WARNING))
    {
        logLevel = LOG_LEVEL_WARNING;
    }
    else if (!strcmp(tempStr, CTAV_LOG_LEVEL_ERROR))
    {
        logLevel = LOG_LEVEL_ERROR;
    }
    else if (!strcmp(tempStr, CTAV_LOG_LEVEL_NO))
    {
        logLevel = LOG_LEVEL_NO;
    }
    else
    {
        log_error("Wrong log level value provided. Available values: "
                  "\"%s\", \"%s\", \"%s\" and \"%s\".",
                  CTAV_LOG_LEVEL_DEBUG, CTAV_LOG_LEVEL_WARNING,
                  CTAV_LOG_LEVEL_ERROR, CTAV_LOG_LEVEL_NO);
        return -1;
    }

    log_setLevel(logLevel);

    tempTag = config_getChildTag(logTag, CT_LOG_USE_SYSLOG, FALSE);
    if (tempTag == NULL)
    {
    	// use printf
    	log_usePrintf();
    }
    else
    {
    	// use syslog
        int facility = LOG_USER;
        // check facility attribute
        tempStr = config_getTagAttributeValue(tempTag, CTA_LOG_FACILITY, FALSE);
        if (tempStr != NULL)
        {
            if (!strcmp(tempStr, CTAV_LOG_FACILITY_USER))
            {
                facility = LOG_USER;
            }
            else if (!strcmp(tempStr, CTAV_LOG_FACILITY_DAEMON))
            {
                facility = LOG_DAEMON;
            }
            else if (!strncmp(tempStr, CTAV_LOG_FACILITY_LOCAL0, 5) &&
                     (strlen(tempStr) == 6))
            {
                // it is probably one of local0 - local7
                switch(tempStr[5])
                {
                case '0':
                    facility = LOG_LOCAL0;
                    break;
                case '1':
                    facility = LOG_LOCAL1;
                    break;
                case '2':
                    facility = LOG_LOCAL2;
                    break;
                case '3':
                    facility = LOG_LOCAL3;
                    break;
                case '4':
                    facility = LOG_LOCAL4;
                    break;
                case '5':
                    facility = LOG_LOCAL5;
                    break;
                case '6':
                    facility = LOG_LOCAL6;
                    break;
                case '7':
                    facility = LOG_LOCAL7;
                    break;
                default:
                    log_error("Wrong log facility value provided. Available "
                              "values: \"user\", \"daemon\", "
                              "\"local0\"-\"local7\".");
                    return -1;
                }
            }
            else // facility value has unknown value
            {
                log_error("Wrong log facility value provided. Available "
                          "values: \"user\", \"daemon\", "
                          "\"local0\"-\"local7\".");
                return -1;
            }
        }

        log_useSyslog(facility);
    }


    log_debug("Log is configured ...");

    return 0;
}
