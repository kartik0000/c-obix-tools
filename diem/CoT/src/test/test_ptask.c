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

int testPtaskReschedule(Task_Thread* thread,
                        const char* testName,
                        long period,
                        BOOL add,
                        BOOL shouldPass)
{
    // schedule task and save it's next execution time
    struct timespec scheduledTime;
    int taskId = ptask_schedule(thread, &testTask, (void*) "10000",
                                10000, EXECUTE_INDEFINITE);
    Periodic_Task* ptask = periodicTask_get(thread, taskId);
    timespec_copy(&scheduledTime, &(ptask->nextScheduledTime));

    // try to reschedule new period
    int error = ptask_reschedule(thread,
                                 taskId,
                                 period,
                                 EXECUTE_INDEFINITE,
                                 add);
    if ((error != 0) && shouldPass)
    {
        printf("ptask_reschedule(thread, taskId, %ld, EXECUTE_INDEFINITE, %d) "
               "returned %d, but it should pass.\n", period, add, error);
        printTestResult(testName, FALSE);
        return 1;
    }
    else if (!shouldPass)
    {
        printf("ptask_reschedule(thread, taskId, %ld, EXECUTE_INDEFINITE, %d) "
               "returned %d. It expected to fail.\n", period, add, error);
        printTestResult(testName, (error != 0) ? TRUE : FALSE);
        return (error != 0) ? 0 : 1;
    }
    // check the new time
    if (!add)
    {
        // if totally new period was set we can only check that new execution
        // time is bigger than previous
        if (timespec_cmp(&scheduledTime, &(ptask->nextScheduledTime)) < 0)
        {
            printf("New scheduled time is less than previous one!\n");
            printTestResult(testName, FALSE);
            return 1;
        }
    }
    else // add == TRUE
    {
        // new scheduled time should be old time + period
        // get the difference between old and new scheduled time
        struct timespec temp;
        timespec_copy(&temp, &(ptask->nextScheduledTime));
        scheduledTime.tv_sec = - scheduledTime.tv_sec;
        scheduledTime.tv_nsec = - scheduledTime.tv_nsec;
        timespec_add(&temp, &scheduledTime);
        // calculate difference in milliseconds
        long difference = (temp.tv_nsec / 1000000) + (temp.tv_sec * 1000);
        if (difference != period)
        {
            printf("Difference between old and new scheduled times is %ld, but"
                   "it should be %ld!\n", difference, period);
            printTestResult(testName, FALSE);
            return 1;
        }
    }

    // cancel task (note that they are not canceled on errors, but that's
    // because I'm too lazy
    ptask_cancel(thread, taskId);

    printTestResult(testName, TRUE);
    return 0;
}

int testPeriodicTask(Task_Thread* thread)
{
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

    printTestResult(testName, TRUE);
    return 0;
}

int test_ptask()
{
    // init environment
    Task_Thread* thread = (Task_Thread*) malloc(sizeof(Task_Thread));
    thread->taskList = NULL;
    thread->id_gen = 0;
    pthread_mutex_init(&(thread->taskListMutex), NULL);
    pthread_cond_init(&(thread->taskListUpdated), NULL);

    int result = 0;

    result += testPeriodicTask(thread);

    // test ptask_reschedule method
    result += testPtaskReschedule(thread,
                                  "test ptask_reschedule: new period is 1010 ",
                                  1010,
                                  FALSE,
                                  TRUE);
    result += testPtaskReschedule(thread,
                                  "test ptask_reschedule: new period is -1010 ",
                                  -1010,
                                  FALSE,
                                  FALSE);
    result += testPtaskReschedule(thread,
                                  "test ptask_reschedule: add 1010 ",
                                  1010,
                                  TRUE,
                                  TRUE);
    result += testPtaskReschedule(thread,
                                  "test ptask_reschedule: add -1010 ",
                                  -1010,
                                  TRUE,
                                  TRUE);
    result += testPtaskReschedule(thread,
                                  "test ptask_reschedule: subtract too much",
                                  -10010,
                                  TRUE,
                                  FALSE);

    // clean environment
    periodicTask_deleteRecursive(thread->taskList);
    thread->taskList = NULL;
    pthread_mutex_destroy(&(thread->taskListMutex));
    pthread_cond_destroy(&(thread->taskListUpdated));


    return result;
}
