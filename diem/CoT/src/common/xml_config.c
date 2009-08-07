/** @file
 * @todo ad description
 *
 * @author Andrey Litvinov
 */
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xml_config.h"
#include "lwl_ext.h"
#include "obix_utils.h"

const char* CT_CONFIG = "config";
const char* CTA_VALUE = "val";

static char* resourceFolder;

static IXML_Document* xmlConfigDoc;

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
    IXML_Element* configTag = config_getChildTag((IXML_Element *)xmlConfigDoc,
                              CT_CONFIG,
                              TRUE);

    if (log_config(configTag) != 0)
        return NULL;

    return configTag;
}

void config_finishInit(BOOL successful)
{
    ixmlDocument_free(xmlConfigDoc);
    if (successful)
    {
        log_debug("\n"
                  "--------------------------------------------------------------------------------\n"
                  "--------------       Initialization completed successfully        --------------\n"
                  "--------------------------------------------------------------------------------");
    }
    else
    {
        log_error("\n"
                  "--------------------------------------------------------------------------------\n"
                  "--------------               Initialization failed                --------------\n"
                  "--------------------------------------------------------------------------------");
    }
}

void config_dispose()
{
    log_dispose();
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

