/*
 * test_part.c
 *
 *  Created on: Mar 20, 2009
 *      Author: andrey
 */
#include <unistd.h>
#include "test_main.h"
#include <lwl_ext.h>

#include <ptask.c>

void testTask(void* arg)
{
    time_t now = time(NULL);
    printf("%sHello, I'm test task. My arg is \"%s\".\n", ctime(&now), (char*) arg) ;
}

int testGetClosestTask(Task_Thread* thread, char* testName, int checkIds[], int iterations)
{
    int i;
    Periodic_Task* ptask;
    for (i = 0; i < iterations; i++)
    {
        ptask = periodicTask_getClosest(thread);
        int taskId = ptask->id;
        periodicTask_execute(thread, ptask);
        if (taskId != checkIds[i])
        {
            printf("Wrong closest task returned: id should be %d, but it is %d.\n",
                   checkIds[i], taskId);
            printTestResult(testName, FALSE);
            return 1;
        }
    }

    printTestResult(testName, TRUE);
    return 0;
}

void taskStopThread(void* arg)
{
    time_t now = time(NULL);
    printf("%sScheduling stop of the thread.\n", ctime(&now));
    ptask_dispose((Task_Thread*) arg);
}

void test_ptask_byHands()
{
    Task_Thread* thread;
    thread = ptask_init();
    time_t now = time(NULL);
    printf("%sScheduling tasks..\n", ctime(&now));
    ptask_schedule(thread, &testTask, (void*) "2,5 seconds",
                   2500, EXECUTE_INDEFINITE);
    ptask_schedule(thread, &testTask, (void*) "3 seconds",
                   3000, EXECUTE_INDEFINITE);
    ptask_schedule(thread, &testTask, (void*) "1 seconds",
                   1000, EXECUTE_INDEFINITE);
    ptask_schedule(thread, &taskStopThread, NULL,
                   10000, EXECUTE_INDEFINITE);
    // wait until all tasks are executed
    sleep(11);
}

int testPeriodicTask()
{
    Task_Thread* thread = (Task_Thread*) malloc(sizeof(Task_Thread));
    thread->taskList = NULL;
    thread->id_gen = 0;
    pthread_mutex_init(&(thread->taskListMutex), NULL);
    pthread_cond_init(&(thread->taskListUpdated), NULL);

    const char* testName = "PeriodicTask test";
    int id200 = ptask_schedule(thread, &testTask, (void*) "200",
                               2001, EXECUTE_INDEFINITE);
    int id300 = ptask_schedule(thread, &testTask, (void*) "300",
                               3001, EXECUTE_INDEFINITE);
    int id50 = ptask_schedule(thread, &testTask, (void*) "50",
                                   502, 2);
    int id90 = ptask_schedule(thread, &testTask, (void*) "90",
                              902, EXECUTE_INDEFINITE);

    // check that tasks are sorted in the list according to their period
    Periodic_Task* ptask = thread->taskList;
    int sortedIds[] = {id50, id90, id200, id300};
    int i;
    for (i = 0; i < 3; i++)
    {
        if (ptask->id != sortedIds[i])
        {
            printf("Tasks are sorted wrongly: id should be %d, but it is %d.\n",
                   sortedIds[i], ptask->id);
            printTestResult(testName, FALSE);
            return 1;
        }
        ptask = ptask->next;
    }


    // test how the closest task is returned.
    int checkIds[] = {id50, id90, id50, id90, id200, id90, id300, id90};
    int error = testGetClosestTask(thread, "test getClosestTask()", checkIds, 8);
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }

    // test removing periodic tasks
    error = ptask_cancel(thread, id200);
    if (error != 0)
    {
        printf("Unable to remove a task: removePeriodicTask() returned error");
        printTestResult(testName, FALSE);
        return 1;
    }
    checkIds[0] = id90;
    checkIds[1] = id90;
    checkIds[2] = id300;
    error = testGetClosestTask(thread, "test removePeriodic task from the middle",
                               checkIds, 3);
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }

    ptask_cancel(thread, id90);
    checkIds[0] = id300;
    checkIds[1] = id300;
    error = testGetClosestTask(thread, "test removePeriodic task from the beginning",
                               checkIds, 2);
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }

    // remove all remaining tasks
    periodicTask_deleteRecursive(thread->taskList);
    thread->taskList = NULL;
    pthread_mutex_destroy(&(thread->taskListMutex));
    pthread_cond_destroy(&(thread->taskListUpdated));

    printTestResult(testName, TRUE);
    return 0;
}

int test_ptask()
{
    int result = 0;

    result += testPeriodicTask();

    return result;
}
