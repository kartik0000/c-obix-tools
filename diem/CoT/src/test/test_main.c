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
 * Entry point for tests.
 *
 * @author Andrey Litvinov
 */
#include <stdio.h>
#include "test_client.h"
#include "test_server.h"
#include "test_common.h"
#include "test_table.h"
#include "test_ptask.h"
#include "test_main.h"

/**
 * Runs automatic (unit) tests.
 * Launches test sets for different modules. When new test module is created,
 * its main function should be invoked from here. If only one module needs to
 * be tested during development, other calls can be commented.
 *
 * @param resFolder Resource folder of the project.
 * @return Number of failed tests.
 */
static int runAutomaticTests(const char* resFolder)
{
    int result = 0;

    result += test_server(resFolder);
    result += test_ptask();
    // requires oBIX server running at localhost
    result += test_client();
    result += test_table();
    result += test_common();


    return result;
}

/**
 * Runs manual tests (tests with user interaction, or manual result analysis).
 * If some new manual test set is created, it should be called from here.
 * Most of the time during development all manual tests should be disabled in
 * order to make test system fully automatic.
 */
static void runManualTests()
{
    // test_ptask_byHands();
    // test_common_byHands();
    // test_client_byHands();
}

void printTestResult(const char* name, BOOL successfull)
{
    printf("--------------------------------------------------------------------------------\n"
           "%s:\t\tTEST: %s.\n"
           "--------------------------------------------------------------------------------\n\n",
           successfull ? "PASSED" : "! FAILED !", name);
}

/** Test entry point. */
int main(int argc, char** argv)
{
    printf("Test is started..\n");

    const char* resFolder = "res/";

    if (argc == 2)
    {
        resFolder = (argv[1]);
    }

    int result = runAutomaticTests(resFolder);

    if (result > 0)
    {
        printf("\n!!!! %d tests failed !!!!\n", result);
        fprintf(stderr, "\n!!!! %d tests failed !!!!\n", result);
    }
    else
    {
        printf("\nTest is successfully completed!\n");
        fprintf(stderr, "\nTest is successfully completed!\n");
    }

    printf("\n\n\n");

    runManualTests();

    return result;
}

