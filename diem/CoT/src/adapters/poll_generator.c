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
 * This is a test application, which generates poll requests to the oBIX server.
 * It can emulate several device adapters who publish some data to the oBIX
 * server and then start polling it. There are three types of poll requests and
 * also a possibility to emulate writing requests, which normal device adapters
 * do in order to update device state at the server.
 * The configuration of the emulator is performed using command line arguments
 * and XML configuration file. The whole list of available command line
 * arguments can be found by launching poll generator with no arguments. The XML
 * configuration template can be found at res/poll_generator_config.xml
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <stdio.h>

#include <obix_client.h>
#include <xml_config.h>
#include <log_utils.h>
#include <ixml_ext.h>

#define USAGE_MESSAGE "\n" \
	"Usage:\n" \
	"   poll_generator params config_file\n" \
	"where\n" \
	" config_file - Name of the configuration file\n" \
	" params = -d count [ -p delay | -P rate ] -t[r|s|l min_d]" \
	          " [ -w delay | -W rate]\n" \
	"\n" \
	"Obligatory parameters:\n" \
	" -d count: \t Number of devices;\n" \
	" -p or -P: \t Define frequency of poll requests:\n" \
	"           \t -p  - Interval between poll requests for each device;\n" \
	"           \t -P  - Total request rate per second generated by the\n" \
	"           \t       application;\n" \
	" -t[r|s|l]:\t Type of generated poll requests:\n" \
	"           \t -tr - Simple read requests;\n" \
	"           \t -ts - Traditional \'short\' polling;\n" \
	"           \t -tl - Long polling; min_d - minimum poll waiting time;\n" \
	"\n" \
	"Optional parameters:\n" \
	" -w or -W: \t Enable emulation of writing requests:\n" \
	"           \t -w  - Interval between writing requests;\n" \
	"           \t -W  - Total writing request rate per second generated by\n" \
	"           \t       the application\n" \
	"\n" \
	"Example:\n" \
	"   poll_generator -d 5 -P 5 -ts config.xml\n" \
	" This command will launch emulation of 5 devices with total poll\n" \
	" request rate = 5 requests per second (just each device will poll\n" \
	" changes from server once in a second).\n" \
	" Polling mode = Traditional 'short' polling (using Watch.pollChanges\n" \
	" operation).\n" \
	" Connection settings will be loaded from \'confix.xml\' file.\n"

typedef enum {
    POLL_T_NONE,
    POLL_T_READ,
    POLL_T_SHORT,
    POLL_T_LONG
} POLL_TYPE;

char* _configFile = NULL;
long _deviceCount = 0;
long _pollInterval = 0;
long _writeInterval = 0;
POLL_TYPE _pollType = POLL_T_NONE;
long _longPollMinInterval = 0;
IXML_Element* _deviceData = NULL;

static BOOL parseLong(long* output,
                      int argumentsCount,
                      char** arguments,
                      int position)
{
    if (*output != 0)
    {
        printf("Duplicate argument \"%s\" is found.\n", arguments[position]);
        return FALSE;
    }

    if ((position + 1) == argumentsCount)
    {
        printf("Argument \"%s\" must be followed with a valid integer "
               "value, which should be greater than 0.\n", arguments[position]);
        return FALSE;
    }

    // try to parse the next argument
    *output = atol(arguments[position + 1]);
    if (*output <= 0)
    {
        printf("Argument \"%s\" must be followed with a valid integer "
               "value, which should be greater than 0.\n", arguments[position]);
        return FALSE;
    }

    return TRUE;
}

static POLL_TYPE parseAndSetPollType(char* argument)
{
    if (_pollType != POLL_T_NONE)
    {
        printf("Polling type should be specified only once "
               "(\"-r\"; \"-s\" or \"-l\").\n");
        return POLL_T_NONE;
    }

    if (strlen(argument) != 3)
    {	// the argument should be '-t'+ 1 character which defines the poll type
        printf("Unknown argument: %s", argument);
        return POLL_T_NONE;
    }

    POLL_TYPE type;

    switch(argument[2])
    {
    case 'r': // Poll data using simple read requests
        type = POLL_T_READ;
        break;
    case 's': // Poll data using short polling
        type = POLL_T_SHORT;
        break;
    case 'l': // Poll data using long polling
        type = POLL_T_LONG;
        break;
    default:
        {
            printf("Unknown argument: %s", argument);
            return POLL_T_NONE;
        }
        break;
    }

    //set pollType
    _pollType = type;
    return type;
}

/**
 * Checks that we have parsed all required arguments and that there is no
 * conflict between them.
 */
static BOOL checkParsedArguments(long pollPerSecond, long writePerSecond)
{
    if (pollPerSecond > 0)
    {
        if (_pollInterval > 0)
        {
            printf("Both polling interval for each device (\"-p\") and "
                   "polling rate per second (\"-P\") can't be provided "
                   "simultaneously.\n");
            return FALSE;
        }

        _pollInterval = 1000 / pollPerSecond * _deviceCount;
    }

    if (writePerSecond > 0)
    {
        if (_writeInterval > 0)
        {
            printf("Both writing interval for each device (\"-w\") and "
                   "writing request rate per second (\"-W\") can't be "
                   "provided simultaneously.\n");
            return FALSE;
        }

        _writeInterval = 1000 / writePerSecond * _deviceCount;
    }

    // now check that the we have enough arguments
    if (_configFile == NULL)
    {
        printf("Configuration file is not specified.\n");
        return FALSE;
    }

    if (_deviceCount == 0)
    {
        printf("Number of devices is not specified (\"-d\").\n");
        return FALSE;
    }

    if (_pollInterval == 0)
    {
        printf("Either polling interval for each device (\"-p\") or "
               "polling rate per second (\"-P\") must be specified.\n");
        return FALSE;
    }

    if (_pollType == POLL_T_NONE)
    {
        printf("Polling type is not specified.\n");
        return FALSE;
    }

    return TRUE;
}

/**
 * Parses input arguments.
 *
 * @param argumentsCount Number of arguments.
 * @param arguments Array of arguments.
 * @return @li @a #TRUE if parsing is complete successfully;
 * 		   @li @a #FALSE on error.
 */
static BOOL parseInputArguments(int argumentsCount, char** arguments)
{
    if (argumentsCount == 1)
    {
        printf("Program cannot be launched without arguments.\n");
        return FALSE;
    }

    long pollPerSecond = 0;
    long writePerSecond = 0;

    // parse input arguments
    int i;
    for (i = 1; i < argumentsCount; i++)
    {
        char* arg = arguments[i];

        if (*arg != '-')
        {
            // consider this as the configuration file name
            _configFile = arg;
            continue;
        }

        // check the first letter after '-'
        switch (arg[1])
        {
        case 'd': // Number of devices
            if (!parseLong(&_deviceCount, argumentsCount, arguments, i))
            {	// parsing failed
                return FALSE;
            }
            break;
        case 'p': // Poll interval for each device
            if (!parseLong(&_pollInterval, argumentsCount, arguments, i))
            {	// parsing failed
                return FALSE;
            }
            break;
        case 'P': // Number of poll requests per second
            if (!parseLong(&pollPerSecond, argumentsCount, arguments, i))
            {	// parsing failed
                return FALSE;
            }
            break;
        case 'w': // Writing request interval for each device
            if (!parseLong(&_writeInterval, argumentsCount, arguments, i))
            {	// parsing failed
                return FALSE;
            }
            break;
        case 'W': // Number of writing requests per second
            if (!parseLong(&writePerSecond, argumentsCount, arguments, i))
            {	// parsing failed
                return FALSE;
            }
            break;
        case 't': // Poll request type
            {
                POLL_TYPE type = parseAndSetPollType(arg);
                if (type == POLL_T_NONE)
                {
                    return FALSE;
                }

                if (type == POLL_T_LONG)
                {
                    // long polling should be also followed with integer value
                    if (!parseLong(&_longPollMinInterval,
                                   argumentsCount,
                                   arguments,
                                   i))
                    {
                        return FALSE;
                    }
                }
            }
            break;
        default:
            {
                printf("Unknown argument: %s\n", arg);
                return FALSE;
            }
            break;
        }
    }

    // parsing is successfully completed
    return checkParsedArguments(pollPerSecond, writePerSecond);
}

static char* getStringFromLong(long value)
{
    // 16 bytes should be enough for any long value
    char* output = (char*) malloc(16);
    if (output == NULL)
    {
        return NULL;
    }
    sprintf(output, "%ld", value);

    return output;
}

static int ixmlElement_setIntegerAttribute(
    IXML_Element* element,
    const char* attrName,
    long attrValue)
{
    char* stringValue = getStringFromLong(tagValue);
    if (stringValue == NULL)
    {
        log_error("Not enough memory.");
        return -1;
    }
    int error = ixmlElement_setAttribute(childTag, attrName, stringValue);
    free(stringValue);
    if (error != IXML_SUCCESS)
    {
        return -1;
    }

    return 0;
}

static int createChildTagWithIntegerValue(IXML_Element* parentTag,
        const char* tagName,
        long tagValue)
{
    IXML_Element* childTag =
        ixmlElement_createChildElementWithLog(parentTag,
                                              tagName);
    if (childTag == NULL)
    {
        return -1;
    }

    ixmlElement_setIntegerAttribute(childTag, CTA_VALUE, tagValue);

    return 0;
}

static int generateConnectionConfig(IXML_Element* configXML)
{
    // config file has one connection config tag
    // we generate connection tag for each virtual device
    int error;

    IXML_Element* connectionTag =
        config_getChildTag(configXML, "connection", TRUE);
    if (connectionTag == NULL)
    {
        return -1;
    }

    // set polling settings
    if (_pollType == POLL_T_SHORT)
    {
        error = createChildTagWithIntegerValue(connectionTag,
                                               "poll-interval",
                                               _pollInterval);
        if (error != 0)
        {
            return error;
        }
    }
    else if (_pollType == POLL_T_LONG)
    {
        IXML_Element* longPollTag =
            ixmlElement_createChildElementWithLog(connectionTag, "long-poll");
        if (longPollTag == NULL)
        {
            return -1;
        }
        error = createChildTagWithIntegerValue(longPollTag,
                                               "min-interval",
                                               _longPollMinInterval);
        error += createChildTagWithIntegerValue(longPollTag,
                                                "max-interval",
                                                _pollInterval);
    }

    if (error != 0)
    {
        log_error("Unable to generate XML with polling settings.");
        return error;
    }

    // clone connection tags: 1 tag per each device
    int i;
    for (i = 1; i < _deviceCount; i++)
    {
        IXML_Node* clonedNode =
            ixmlNode_cloneNode(ixmlElement_getNode(connectionTag), TRUE);
        ixmlNode_appendChild(ixmlElement_getNode(configXML), clonedNode);
        // update it's id attribute
        error = ixmlElement_setIntegerAttribute(
                    ixmlNode_convertToElement(clonedNode),
                    "id",
                    i);
        if (error != 0)
        {
            return error;
        }
    }

    return 0;
}

/**
 * Loads configuration file, generates XML for the oBIX Client Library and
 * initializes it.
 */
static int loadConfigFile()
{
    IXML_Element* configXML = config_loadFile(_configFile);
    if (configXML == NULL)
    {
        return -1;
    }

    int error = config_log(configXML);
    if (error != 0)
    {
        config_finishInit(configXML, FALSE);
        return error;
    }

    // initialize oBIX client library with connection settings
    error = generateConnectionConfig(configXML);
    if (error != 0)
    {
        config_finishInit(configXML, FALSE);
        return error;
    }

    error = obix_loadConfig(configXML);
    if (error != OBIX_SUCCESS)
    {
        config_finishInit(configXML, FALSE);
        return error;
    }

    // load device data which will be posted to the oBIX server by each device
    _deviceData = config_getChildTag(configXML, "device-info", TRUE);
    if (_deviceData == NULL)
    {
        config_finishInit(configXML, FALSE);
        return error;
    }
    // clone this data because it should be available after config file is
    // closed
    _deviceData = ixmlElement_cloneWithLog(_deviceData);
    if (_deviceData == NULL)
    {
        config_finishInit(configXML, FALSE);
        return error;
    }

    config_finishInit(configXML, TRUE);
    return 0;
}

int main(int argumentsCount, char** arguments)
{
    int error;
    printf("oBIX poll request generator v. 1.0\n");

    if (!parseInputArguments(argumentsCount, arguments))
    {
        printf(USAGE_MESSAGE);
        return -1;
    }

    error = loadConfigFile();
    if (error != 0)
    {
        return error;
    }

    return 0;
}
