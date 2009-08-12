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
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log_utils.h>
#include <xml_config.h>
#include <obix_utils.h>
#include "test_main.h"

void testLog()
{
    log_debug("testing debug 4=%d;", 4);
    log_warning("testing warning 5=%d;", 5);
    log_error("testing %s 4=%d;", "error", 4);
    IXML_Element* settings = config_loadFile("server_config.xml");
    config_log(settings);
    config_finishInit(settings, TRUE);
    log_debug("testing debug 4=%d;", 4);
    log_warning("testing warning 5=%d;", 5);
    log_error("testing %s 4=%d;", "error", 4);
    log_usePrintf();
}


int testObix_reltime_parseToLong(const char* reltime, int retVal, long value)
{
    long l = 0;
    int error = obix_reltime_parseToLong(reltime, &l);
    if (error != retVal)
    {
        printf("obix_reltime_parseToLong(\"%s\", %d, %ld) returned %d, but "
               "should return %d.\n", reltime, retVal, value, error, retVal);
        return 1;
    }
    if (l != value)
    {
        printf("obix_reltime_parseToLong(\"%s\", %d, %ld) parsed %ld, but it "
               "should be %ld.\n", reltime, retVal, value, l, value);
        return 1;
    }

    return 0;
}

int testObix_reltime_parse()
{
    int error = 0;

    // general test
    error += testObix_reltime_parseToLong("-P2DT1H2M3.005S", 0, - ((((2 * 24 + 1) * 60 + 2) * 60 + 3) * 1000 + 5));
    error += testObix_reltime_parseToLong("P1DT2S", 0, (1 * 24 * 60 * 60 + 2) * 1000);
    error += testObix_reltime_parseToLong("P0DT2S", 0, 2000);
    error += testObix_reltime_parseToLong("PT0H2S", 0, 2000);
    error += testObix_reltime_parseToLong("P1D", 0, 24 * 60 * 60 * 1000);
    error += testObix_reltime_parseToLong("PT1H", 0, 60 * 60 * 1000);
    error += testObix_reltime_parseToLong("PT1M", 0, 60000);
    error += testObix_reltime_parseToLong("PT1H0.1S", 0, 3600100);
    error += testObix_reltime_parseToLong("P0D", 0, 0);
    error += testObix_reltime_parseToLong("PT0H", 0, 0);
    error += testObix_reltime_parseToLong("PT0M", 0, 0);
    error += testObix_reltime_parseToLong("PT0S", 0, 0);

    // check parsing milliseconds
    error += testObix_reltime_parseToLong("PT0.05S", 0, 50);
    error += testObix_reltime_parseToLong("PT0.5S", 0, 500);
    error += testObix_reltime_parseToLong("PT0.505S", 0, 505);
    error += testObix_reltime_parseToLong("PT0.50555S", 0, 505);

    // check wrong format detection
    error += testObix_reltime_parseToLong("PT-1S", -1, 0);
    error += testObix_reltime_parseToLong("PT1.S", -1, 0);
    error += testObix_reltime_parseToLong("PT.1S", -1, 0);
    error += testObix_reltime_parseToLong("PT2.1M", -1, 0);
    error += testObix_reltime_parseToLong("PT", -1, 0);
    error += testObix_reltime_parseToLong("PTS", -1, 0);
    error += testObix_reltime_parseToLong("PTH2S", -1, 0);
    error += testObix_reltime_parseToLong("PT2HS", -1, 0);
    error += testObix_reltime_parseToLong("PD", -1, 0);
    error += testObix_reltime_parseToLong("T", -1, 0);
    error += testObix_reltime_parseToLong("P2DT", -1, 0);

    // check overflow detection
    error += testObix_reltime_parseToLong("P1Y", -2, 0);
    error += testObix_reltime_parseToLong("P1M", -2, 0);
    error += testObix_reltime_parseToLong("P24D", -2, 0);
    error += testObix_reltime_parseToLong("P23DT99H", -2, 0);
    error += testObix_reltime_parseToLong("PT1H123456M", -2, 0);
    error += testObix_reltime_parseToLong("PT999999999999S", -2, 0);
    error += testObix_reltime_parseToLong("PT111111111111S", -2, 0);
    // maximum that can be parsed: almost 24 days (23:59:59.999)
    int l = ((((23 * 24 + 23) * 60 + 59) * 60 + 59) * 1000 + 999);
    error += testObix_reltime_parseToLong("P23DT23H59M59.999S", 0, l);

    printTestResult("Test obix_reltime_parse*", (error == 0) ? TRUE : FALSE);

    return error;
}

int testObix_reltime_fromLong()
{

    int testObix_reltime_fromLongHelper(long period,
                                        RELTIME_FORMAT format,
                                        const char* checkString)
    {
        char* reltime = obix_reltime_fromLong(period, format);

        if (strcmp(reltime, checkString) != 0)
        {
            printf("obix_reltime_fromLong(%ld, %d) generated \"%s\", but "
                   "expected \"%s\".\n", period, format, reltime, checkString);
            free(reltime);
            return 1;
        }

        free(reltime);
        return 0;
    }

    int error = 0;

    error += testObix_reltime_fromLongHelper(
                 ((((25 * 60) + 1) * 60) + 1) * 1000 + 10,
                 RELTIME_YEAR,
                 "P1DT1H1M1.01S");
    error += testObix_reltime_fromLongHelper(
                 - (((((25 * 60) + 1) * 60) + 1) * 1000 + 10),
                 RELTIME_HOUR,
                 "-PT25H1M1.01S");
    error += testObix_reltime_fromLongHelper(
                 24 * 60 * 60 * 1000,
                 RELTIME_DAY,
                 "P1D");
    error += testObix_reltime_fromLongHelper(
                 65 * 1000,
                 RELTIME_DAY,
                 "PT1M5S");
    error += testObix_reltime_fromLongHelper(
                 60 * 60 * 1000 + 100,
                 RELTIME_DAY,
                 "PT1H0.1S");
    error += testObix_reltime_fromLongHelper(
                     10000 * 1000 + 1,
                     RELTIME_SEC,
                     "PT10000.001S");

    printTestResult("Test obix_reltime_fromLong()", (error == 0) ? TRUE : FALSE);

    return error;
}

int testObixUtils()
{
    int result = 0;

    result += testObix_reltime_parse();
    result += testObix_reltime_fromLong();

    return result;
}

int test_common()
{
    int result = 0;

    result += testObixUtils();

    return result;
}

void test_common_byHands()
{
    testLog();
}
