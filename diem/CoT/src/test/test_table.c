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
#include <string.h>
#include <time.h>
#include <table.h>
#include "test_main.h"
#include "test_table.h"

static int testTableGet(Table* table, const char* key, const char* value)
{
    const char* testName = "Test table_get()";
    char* val = (char*) table_get(table, key);
    if (val == NULL)
    {
        printf("Unable to find key \"%s\".\n", key);
        printTestResult(testName, FALSE);
        return 1;
    }

    if (strcmp(val, value) != 0)
    {
        printf("Returned value (\"%s\") doesn't correspond to pair"
               " \"%s\" : \"%s\".\n", val, key, value);
        printTestResult(testName, FALSE);
        return 1;
    }

    return 0;
}

int test_table()
{
    const char* testName = "Test table.c";

    Table* table = table_create(3);
    if (table == NULL)
    {
        printf("Unable to create table: table_create() returned NULL.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    // try to put more values than size if the table
    struct timespec startTime, endTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    // TODO check here some problem with time at router box
//    fafasfzszdfb
    printf("started at %ld seconds %ld nanoseconds.\n",
    		startTime.tv_sec, startTime.tv_nsec);
    int error = table_put(table, "123", "1 2 3");
    error += table_put(table, "124", "1 2 4");
    error += table_put(table, "125", "1 2 5");
    error += table_put(table, "126", "1 2 6");

    if (error != 0)
    {
        printf("Unable to put to the table: "
               "table_put() calls returned %d.\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    // try to get now few of them back
    error = testTableGet(table, "126", "1 2 6");
    error += testTableGet(table, "123", "1 2 3");
    error += testTableGet(table, "125", "1 2 5");
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }

    // try to delete few
    error = table_remove(table, "123");
    error += table_remove(table, "125");
    if (error != 0)
    {
        printf("Unable to remove from the table: "
               "table_remove() calls returned %d.\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }
    // try to get removed element
    void* val = table_get(table, "123");
    if (val != NULL)
    {
        printf("Unable to remove from the table: "
               "table_get() still returns something for the removed key.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    // add new and try to get once again
    error = table_put(table, "127", "1 2 7");
    if (error != 0)
    {
        printf("Unable to put to the table: "
               "table_put() returned %d.\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }
    error = testTableGet(table, "126", "1 2 6");
    error += testTableGet(table, "124", "1 2 4");
    error += testTableGet(table, "127", "1 2 7");
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);

    endTime.tv_sec -= startTime.tv_sec;
    if (endTime.tv_nsec < startTime.tv_nsec)
    {
        endTime.tv_sec--;
        endTime.tv_nsec = 1000000000L - startTime.tv_nsec + endTime.tv_nsec;
    }
    else
    {
        endTime.tv_nsec -= startTime.tv_nsec;
    }
    printf("Test operations took %ld seconds, %ld nanoseconds.\n",
           endTime.tv_sec, endTime.tv_nsec);

    table_free(table);

    printTestResult(testName, TRUE);
    return 0;
}
