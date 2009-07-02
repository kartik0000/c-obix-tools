#include <stdio.h>
#include "test_client.h"
#include "test_server.h"
#include "test_common.h"
#include "test_table.h"
#include "test_ptask.h"
#include "test_main.h"

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

static void runManualTests()
{
    // test_ptask_byHands();
    // test_common_byHands();
//	test_client_byHands();
}

void printTestResult(const char* name, BOOL successfull)
{
    printf("--------------------------------------------------------------------------------\n"
           "%s:\t\tTEST: %s.\n"
           "--------------------------------------------------------------------------------\n\n",
           successfull ? "PASSED" : "!FAILED!", name);
}

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
