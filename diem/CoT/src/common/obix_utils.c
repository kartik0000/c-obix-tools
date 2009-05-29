/** @file
 * Contains names of oBIX objects, contracts, facets, etc.
 * (<a href="http://obix.org/">http://obix.org/</a>)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "obix_utils.h"

const char* OBIX_HREF_ERR_BAD_URI = "obix:BadUriErr";
const char* OBIX_HREF_ERR_UNSUPPORTED = "obix:UnsupportedErr";
const char* OBIX_HREF_ERR_PERMISSION = "obix:PermissionErr";

const char* OBIX_OBJ = "obj";
const char* OBIX_OBJ_REF = "ref";
const char* OBIX_OBJ_OP = "op";
const char* OBIX_OBJ_LIST = "list";
const char* OBIX_OBJ_ERR = "err";
const char* OBIX_OBJ_BOOL = "bool";
const char* OBIX_OBJ_INT = "int";
const char* OBIX_OBJ_REAL = "real";
const char* OBIX_OBJ_STR = "str";
const char* OBIX_OBJ_ENUM = "enum";
const char* OBIX_OBJ_ABSTIME = "abstime";
const char* OBIX_OBJ_RELTIME = "reltime";
const char* OBIX_OBJ_URI = "uri";
const char* OBIX_OBJ_FEED = "feed";

const char* OBIX_NAME_SIGN_UP = "signUp";
const char* OBIX_NAME_WATCH_SERVICE = "watchService";
const char* OBIX_NAME_WATCH_SERVICE_MAKE = "make";
const char* OBIX_NAME_WATCH_ADD = "add";
const char* OBIX_NAME_WATCH_REMOVE = "remove";
const char* OBIX_NAME_WATCH_POLLCHANGES = "pollChanges";
const char* OBIX_NAME_WATCH_POLLREFRESH = "pollRefresh";
const char* OBIX_NAME_WATCH_DELETE = "delete";
const char* OBIX_NAME_WATCH_LEASE = "lease";

const char* OBIX_OBJ_ERR_TEMPLATE = "<err displayName=\"Internal Server Error\" "
                                    "display=\"%s/>\"";
const char* OBIX_OBJ_NULL_TEMPLATE = "<obj null=\"true\"/>";

const char* OBIX_ATTR_IS = "is";
const char* OBIX_ATTR_NAME = "name";
const char* OBIX_ATTR_HREF = "href";
const char* OBIX_ATTR_VAL = "val";
const char* OBIX_ATTR_WRITABLE = "writable";
const char* OBIX_ATTR_DISPLAY = "display";
const char* OBIX_ATTR_DISPLAY_NAME = "displayName";

const char* XML_TRUE = "true";
const char* XML_FALSE = "false";

int obix_reltime_parseToLong(const char* str, long* period)
{
    int negativeFlag = 0;
    int parsedSomething = 0;
    long result = 0;
    const char* startPos = str;
    char* endPos;

    // parsing xs:duration string. Format: {-}PnYnMnDTnHnMnS
    // where n - amount of years, months, days, hours, minutes and seconds
    // respectively. Seconds can have 3 fraction digits after '.' defining
    // milliseconds.

    /**
     * Parses positive integer. Does the same as strtol(startPos, endPos, 10),
     * but assumes that negative result is an error. Thus returns -1 instead of
     * 0, when nothing is parsed. Also sets flag parsedSomething if parsing
     * did not fail.
     */
    long strtoposl(const char* startPos, char** endPos)
    {
        long l = strtol(startPos, endPos, 10);
        if (startPos == *endPos)
        {
            return -1;
        }
        parsedSomething = 1;

        return l;
    }

    if (*startPos == '-')
    {
        negativeFlag = 1;
        startPos++;
    }

    if (*startPos != 'P')
    {
        return -1;
    }
    startPos++;

    long l = strtoposl(startPos, &endPos);

    // if we parsed years or months...
    if ((*endPos == 'Y') || (*endPos == 'M'))
    {
        // we can't convert years and months to milliseconds it will overflow
        // long type. Maximum that long can hold is 24 days.
        // ( 24 days * 24 hours * 60 min * 60 sec * 1000 ms < 2 ^ 31 )
        return -2;
    }

    // if we parsed days...
    if (*endPos == 'D')
    {
        if (l < 0)
        {	// nothing is parsed, but a number must be specified before 'D'
            // or negative number is parsed
            return -1;
        }
        // again check for buffer overflow (check the comments above)
        if (l > 23)
        {
            return -2;
        }

        result += l * 86400000;

        startPos = endPos + 1;
    }

    if (*startPos == 'T')
    {
        // we reached time section
        startPos++;
        // reset parsing flag, because there must be something after 'T'
        parsedSomething = 0;

        l = strtoposl(startPos, &endPos);

        // if we parsed hours...
        if (*endPos == 'H')
        {
            if ( l < 0)
            {	// 'H' occurred, but there was no number before it
                return -1;
            }
            // if days already parsed, than restrict hours to be < 24,
            // otherwise, hours must be < 595 (or it will overflow long)
            if ((l > 595) || ((result > 0) && (l > 23)))
            {
                return -2;
            }

            result += l * 3600000;

            // parse further
            startPos = endPos + 1;
            l = strtoposl(startPos, &endPos);
        }

        // if we parsed minutes...
        if (*endPos == 'M')
        {
            if ( l < 0)
            {	// 'M' occurred, but there was no number before it
                return -1;
            }
            // if something is already parsed, than restrict minutes to be < 60
            // otherwise, minutes must be < 35790 (or it will overflow long)
            if ((l > 35790) || ((result > 0) && (l > 59)))
            {
                return -2;
            }

            result += l * 60000;

            // parse further
            startPos = endPos + 1;
            l = strtoposl(startPos, &endPos);
        }

        // if we parsed seconds...
        if ((*endPos == 'S') || (*endPos == '.'))
        {
            if ( l < 0)
            {	// 'S' occurred, but there was no number before it
                return -1;
            }
            // if something is already parsed, than restrict seconds to be < 60
            // otherwise, seconds must be < 2147482 (or it will overflow long)
            if ((l > 2147482) || ((result > 0) && (l > 59)))
            {
                return -2;
            }

            result += l * 1000;

            if (*endPos == '.')
            {
                // parse also milliseconds
                startPos = endPos + 1;
                // reset parsed flag because there always should be
                // something after '.'
                parsedSomething = 0;
                l = strtoposl(startPos, &endPos);
                if (*endPos != 'S')
                {
                    // only seconds can have fraction point
                    return -1;
                }

                // we parsed fraction which is displaying milliseconds -
                // drop everything that is smaller than 0.001
                while (l > 1000)
                {
                    l /= 10;
                }
                // if we parsed '.5', than it should be 500
                int parsedDigits = endPos - startPos;
                for (; parsedDigits < 3; parsedDigits++)
                {
                    l *= 10;
                }

                result += l;
            }
        }
    }

    if (parsedSomething == 0)
    {
    	// we did not parse any value
    	return -1;
    }

    // save result at the output variable
    *period = (negativeFlag == 0) ? result : -result;

    return 0;
}

char* obix_reltime_fromLong(long millis, RELTIME_FORMAT format)
{
    // helper function which calculates required length of a string for
    // storing positive integer value + one symbol
    int plonglen(long l)
    {
        // quite nice stuff, don't you think so? :)
        // but I think it is better than divide/multiply number on each step.
        const long size[] =
            {
                0, 9, 99, 999, 9999, 99999, 999999,
                9999999, 99999999, 999999999, LONG_MAX
            };

        int length;

        for (length = 0; l > size[length]; length++)
            ;

        // reserve one more byte for terminating symbol
        if (length > 0)
        {
            length++;
        }

        return length;
    }

    int days = 0;
    int hours = 0;
    long minutes = 0;
    double seconds = 0;
    int negativeFlag = 0;

    int stringSize = 3;

    if (millis == 0)
    {
        // quick shortcut for zero case
        char* reltime = (char*) malloc(5);
        if (reltime == NULL)
        {
            return NULL;
        }

        strcpy(reltime, "PT0S");
        return reltime;
    }

    if (millis < 0)
    {
        millis = - millis;
        negativeFlag = 1;
        stringSize++;
    }

    seconds = (double) millis / 1000;
    millis %= 1000;

    if (format >= RELTIME_MIN)
    {
        minutes = seconds / 60;
        seconds -= minutes * 60;

        if (format >= RELTIME_HOUR)
        {
            hours = minutes / 60;
            minutes %= 60;

            if (format >= RELTIME_DAY)
            {
                days = hours / 24;
                hours %= 24;
            }
        }
    }

    stringSize += plonglen(days);
    stringSize += plonglen(hours);
    stringSize += plonglen(minutes);
    stringSize += plonglen(seconds);
    if (millis != 0)
    {
    	stringSize += 4;
    }

    char* reltime = (char*) malloc(stringSize);
    if (reltime == NULL)
    {
        return NULL;
    }

    // generating string of a kind 'PnDTnHnMnS'
    int pos = 0;
    if (negativeFlag == 1)
    {
        reltime[pos++] = '-';
    }

    // obligatory symbol
    reltime[pos++] = 'P';

    if (days > 0)
    {
        pos += sprintf(reltime + pos, "%dD", days);
    }

    if ((hours > 0) || (minutes > 0) || (seconds > 0))
    {
        reltime[pos++] = 'T';
    }

    if (hours > 0)
    {
        pos += sprintf(reltime + pos, "%dH", hours);
    }

    if (minutes > 0)
    {
        pos += sprintf(reltime + pos, "%ldM", minutes);
    }

    if (seconds > 0)
    {
    	sprintf(reltime + pos, "%gS", seconds);
    }

    return reltime;

    //    switch (format)
    //    {
    //    case RELTIME_YEAR:
    //    case RELTIME_MONTH:
    //    case RELTIME_DAY:
    //        {
    //
    //            days = millis / 86400000;
    //            millis %= 86400000;
    //        }
    //    case RELTIME_HOUR:
    //        {
    //            hours = millis / 3600000;
    //            millis %= 360000;
    //        }
    //    case RELTIME_MIN:
    //        {
    //            minutes = millis / 60000;
    //            millis %= 60000;
    //        }
    //    case RELTIME_SEC:
    //    case RELTIME_MILLIS:
    //        {
    //            seconds = millis / 1000.0;
    //            //            millis %= 1000;
    //        }
    //    }
}