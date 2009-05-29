/**
 * TODO: write shorcuts for logging, make everything smooth and start
 * implementing the load of specific parameters
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
/*@}*/

static char* resourceFolder;

static IXML_Document* xmlConfigDoc;

//static IXML_Element* getElement(IXML_NodeList* list, BOOL obligatory)
//{
//	ixml
//}

IXML_Element* config_getChildTag(IXML_Element* conf, const char* tagName, BOOL obligatory)
{
    IXML_NodeList* list = ixmlElement_getElementsByTagName(conf, tagName);
    if (list == NULL)
    {
        if (obligatory == TRUE)
        {
            log_error("Obligatory configuration tag <%s> is not found.", tagName);
        }
        return NULL;
    }
    if (ixmlNodeList_length(list) > 1)
    {
        log_warning("All extra <%s> tags are ignored.", tagName);
    }

    IXML_Node* node = ixmlNodeList_item(list, 0);
    ixmlNodeList_free(list);

    if (ixmlNode_getNodeType(node) != eELEMENT_NODE)
    {
        if (obligatory == TRUE)
        {
            log_error("Obligatory configuration tag <%s> is not found.", tagName);
        }
        return NULL;
    }

    return (IXML_Element*) node;
}

const char* config_getChildTagValue(IXML_Element* conf, const char* tagName, BOOL obligatory)
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

int config_getTagIntAttrValue(IXML_Element* tag, const char* attrName, BOOL obligatory, int defaultValue)
{
    long val = config_getTagLongAttrValue(tag, attrName, obligatory, defaultValue);
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

long config_getTagLongAttrValue(IXML_Element* tag, const char* attrName, BOOL obligatory, long defaultValue)
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

int config_getTagBoolAttrValue(IXML_Element* tag, const char* attrName, BOOL obligatory)
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

//IXML_NodeList* config_getChildNodes(IXML_Document* doc, const char* nodeName)
//{
//	IXML_Node* node = config_getChildNode(doc, nodeName, TRUE);
//    if (node == NULL)
//        return NULL;
//
//    return ixmlNode_getChildNodes(node);
//}

IXML_Element* config_loadFile(const char* filename)
{
    char* path = config_getResFullPath(filename);

    //Trying to load the configuration file
    int error = ixmlLoadDocumentEx(path, &xmlConfigDoc);

    if (error != IXML_SUCCESS)
    {
        log_error("Error reading the configuration file \'%s\' (code %i)", path, error);
        free(path);
        return NULL;
    }
    free(path);

    //get the root configure tag from the document
    IXML_Element* configTag = config_getChildTag((IXML_Element *)xmlConfigDoc, CT_CONFIG, TRUE);

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
    {	// resource folder is not set, so we consider that it is a current one
        return strdup(filename);
    }

    // concatenate filename with resource folder
    char* output = (char*) malloc(strlen(filename) + strlen(resourceFolder) + 1);
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
