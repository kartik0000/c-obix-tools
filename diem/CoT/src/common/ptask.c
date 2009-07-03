/** @file
 *
 * @todo Add description
 *
 * @author Andrey Litvinov
 * @version 1.0
 */
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <lwl_ext.h>

#include "ptask.h"

typedef struct _Periodic_Task
{
    int id;
    struct timespec nextScheduledTime;
    struct timespec period;
    int executeTimes;
    periodic_task task;
    void* arg;
    BOOL isCancelled;
    BOOL isExecuting;
    struct _Periodic_Task* prev;
    struct _Periodic_Task* next;
}
Periodic_Task;

struct _Task_Thread
{
    int id_gen;
    Periodic_Task* taskList;
    pthread_t thread;
    pthread_mutex_t taskListMutex;
    pthread_cond_t taskListUpdated;
    pthread_cond_t taskExecuted;
};

/**@name Utility methods for work with @a timespec structure @{*/
static void timespec_fromMillis(struct timespec* time, long millis)
{
    time->tv_sec = millis / 1000;
    time->tv_nsec = (millis % 1000) * 1000000;
}

static int timespec_add(struct timespec* time1, const struct timespec* time2)
{
    long newNano = time1->tv_nsec + time2->tv_nsec;
    time1->tv_sec += time2->tv_sec + (newNano / 1000000000);
    time1->tv_nsec = newNano % 1000000000;
    // check negative cases
    if ((time1->tv_nsec < 0) && (time1->tv_sec > 0))
    {
        time1->tv_sec--;
        time1->tv_nsec = 1000000000 + time1->tv_nsec;
    }
    else if ((time1->tv_sec < 0) && (time1->tv_nsec > 0))
    {
        time1->tv_sec++;
        time1->tv_nsec = time1->tv_nsec - 1000000000;
    }

    // return positive value if result is positive and vice versa
    if ((time1->tv_sec >= 0) && (time1->tv_nsec >= 0))
        return 1;
    else
        return -1;
}

static void timespec_copy(struct timespec* dest, const struct timespec* source)
{
    dest->tv_sec = source->tv_sec;
    dest->tv_nsec = source->tv_nsec;
}

static int timespec_cmp(struct timespec* time1, struct timespec* time2)
{
    // compare seconds first
    if (time1->tv_sec > time2->tv_sec)
    {
        return 1;
    }
    else if (time1->tv_sec < time2->tv_sec)
    {
        return -1;
    }
    else // time1->tv_sec == time2->tv_sec
    {	// compare also nanoseconds
        if (time1->tv_nsec > time2->tv_nsec)
        {
            return 1;
        }
        else if (time1->tv_nsec < time2->tv_nsec)
        {
            return -1;
        }
        else // time1->tv_nsec == time2->tv_nsec
        {
            return 0;
        }
    }
}
/**@}*/

// this is called only from ptask_schedule() thus is thread safe
static int generateId(Task_Thread* thread)
{
    return thread->id_gen++;
}

static void periodicTask_generateNextExecTime(Periodic_Task* ptask)
{
    // we generate next time, by adding period to last time when the task
    // supposed to be executed
    timespec_add(&(ptask->nextScheduledTime), &(ptask->period));
}

static void periodicTask_setPeriod(Periodic_Task* ptask, long period, int executeTimes)
{
    timespec_fromMillis(&(ptask->period), period);
    ptask->executeTimes = executeTimes;
}

static void periodicTask_resetExecTime(Periodic_Task* ptask)
{
    // set next execution time = current time + period
    clock_gettime(CLOCK_REALTIME, &(ptask->nextScheduledTime));
    periodicTask_generateNextExecTime(ptask);
}

static Periodic_Task* periodicTask_create(Task_Thread* thread, periodic_task task, void* arg, long period, int executeTimes)
{
    Periodic_Task* ptask = (Periodic_Task*) malloc(sizeof(Periodic_Task));
    if (ptask == NULL)
    {
        log_error("Unable to create new periodic task: Not enough memory.");
        return NULL;
    }

    ptask->task = task;
    ptask->arg = arg;
    ptask->id = generateId(thread);
    ptask->isCancelled = FALSE;
    ptask->isExecuting = FALSE;
    ptask->prev = NULL;
    ptask->next = NULL;
    periodicTask_setPeriod(ptask, period, executeTimes);
    periodicTask_resetExecTime(ptask);

    return ptask;
}

static void periodicTask_free(Periodic_Task* ptask)
{
    free(ptask);
}

static void periodicTask_removeFromList(Task_Thread* thread, Periodic_Task* ptask)
{
    if (ptask->prev != NULL)
    {
        ptask->prev->next = ptask->next;
    }
    else
    {	// removed task from the top of the list
        thread->taskList = ptask->next;
    }

    if (ptask->next != NULL)
    {
        ptask->next->prev = ptask->prev;
    }
}

// note that this is called only from synchronized context
// i.e. thread->taskListMutex should be locked
// after method execution this mutex remains locked
static void periodicTask_execute(Task_Thread* thread, Periodic_Task* ptask)
{
    ptask->isExecuting = TRUE;
    // release mutex, because execution may take considerable time
    pthread_mutex_unlock(&(thread->taskListMutex));
    (ptask->task)(ptask->arg);
    pthread_mutex_lock(&(thread->taskListMutex));
    // check whether this task was already deleted
    if (ptask->isCancelled == TRUE)
    {
        // no need update the task, because it is already canceled
        periodicTask_free(ptask);
        // notify that task execution is completed in case if ptask_cancel is
        // waiting for it.
        pthread_cond_signal(&(thread->taskExecuted));
        return;
    }
    ptask->isExecuting = FALSE;

    // check whether we need to schedule next execution time
    if (ptask->executeTimes > 0)
    {
        ptask->executeTimes--;
    }

    if (ptask->executeTimes == 0)
    {
        // the task is already executed required times. Remove it
        periodicTask_removeFromList(thread, ptask);
        // and free resources
        periodicTask_free(ptask);
    }
    else
    {
        // schedule next execution
        periodicTask_generateNextExecTime(ptask);
    }
}

static Periodic_Task* periodicTask_get(Task_Thread* thread, int id)
{
    Periodic_Task* ptask = thread->taskList;
    while((ptask != NULL) && (ptask->id != id))
    {
        ptask = ptask->next;
    }
    return ptask;
}

int ptask_schedule(Task_Thread* thread, periodic_task task, void* arg, long period, int executeTimes)
{
    if (executeTimes == 0)
    {	// executeTimes can't be 0, but can be -1 (EXECUTE_INDEFINITE).
        return -1;
    }

    pthread_mutex_lock(&(thread->taskListMutex));
    Periodic_Task* ptask = periodicTask_create(thread, task, arg, period, executeTimes);
    if (ptask == NULL)
    {
        return -1;
    }
    int taskId = ptask->id;

    // put the task to the list.
    if (thread->taskList == NULL)
    {
        thread->taskList = ptask;
        // task list is updated, notify taskThread
        pthread_cond_signal(&(thread->taskListUpdated));
        pthread_mutex_unlock(&(thread->taskListMutex));
        return taskId;
    }

    // we try to keep list sorted according to the period time so that tasks
    // which are executed more often were in the beginning of the list
    Periodic_Task* parent = NULL;
    Periodic_Task* child = thread->taskList;
    while ((child != NULL) &&
            (timespec_cmp(&(child->period), &(ptask->period)) == -1))
    {
        parent = child;
        child = child->next;
    }

    if (parent != NULL)
    {
        parent->next = ptask;
        ptask->prev = parent;
    }
    else
    {	// adding to the top of the list
        thread->taskList = ptask;
    }

    if (child != NULL)
    {
        ptask->next = child;
        child->prev = ptask;
    }

    // task list is updated, notify taskThread
    pthread_cond_signal(&(thread->taskListUpdated));
    pthread_mutex_unlock(&(thread->taskListMutex));

    return taskId;
}

int ptask_reschedule(Task_Thread* thread,
                     int taskId,
                     long period,
                     int executeTimes,
                     BOOL add)
{
    if (executeTimes == 0)
    {	// executeTimes can't be 0, but can be -1 (EXECUTE_INDEFINITE).
        return -1;
    }

    pthread_mutex_lock(&(thread->taskListMutex));

    Periodic_Task* ptask = periodicTask_get(thread, taskId);
    if (ptask == NULL)
    {	// task is not found
        pthread_mutex_unlock(&(thread->taskListMutex));
        return -1;
    }

    if (add)
    {
        // add provided time to the period and next execution time
        struct timespec addTime, temp;
        timespec_fromMillis(&addTime, period);
        timespec_copy(&temp, &(ptask->period));
        if (timespec_add(&temp, &addTime) < 0)
        {	// resulting new period would be negative -> error
            pthread_mutex_unlock(&(thread->taskListMutex));
            return -1;
        }

        timespec_copy(&(ptask->period), &temp);
        timespec_add(&(ptask->nextScheduledTime), &addTime);
        ptask->executeTimes = executeTimes;
    }
    else // add == false
    {
        if (period < 0)
        {
            // period should not be negative
            pthread_mutex_unlock(&(thread->taskListMutex));
            return -1;
        }
        // set provided time as a new period and reschedule execution time
        periodicTask_setPeriod(ptask, period, executeTimes);
        periodicTask_resetExecTime(ptask);
    }

    // task list is updated, notify taskThread
    pthread_cond_signal(&(thread->taskListUpdated));
    pthread_mutex_unlock(&(thread->taskListMutex));
    return 0;
}

BOOL ptask_isScheduled(Task_Thread* thread, int taskId)
{
    if (periodicTask_get(thread, taskId) != NULL)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

int ptask_reset(Task_Thread* thread, int taskId)
{
    pthread_mutex_lock(&(thread->taskListMutex));

    Periodic_Task* ptask = periodicTask_get(thread, taskId);
    if (ptask == NULL)
    {	// task is not found
        pthread_mutex_unlock(&(thread->taskListMutex));
        return -1;
    }

    periodicTask_resetExecTime(ptask);

    // task list is updated, notify taskThread
    pthread_cond_signal(&(thread->taskListUpdated));
    pthread_mutex_unlock(&(thread->taskListMutex));

    return 0;
}

int ptask_cancel(Task_Thread* thread, int taskId, BOOL wait)
{
    pthread_mutex_lock(&(thread->taskListMutex));

    Periodic_Task* ptask = periodicTask_get(thread, taskId);
    if (ptask == NULL)
    {	// task is not found
        pthread_mutex_unlock(&(thread->taskListMutex));
        return -1;
    }

    periodicTask_removeFromList(thread, ptask);
    // task list is updated, notify taskThread
    pthread_cond_signal(&(thread->taskListUpdated));

    if (ptask->isExecuting)
    {   // can't delete the task right now, mark it as canceled
        ptask->isCancelled = TRUE;
        if (wait)
        {	// wait until the task is completed and really removed.
            pthread_cond_wait(&(thread->taskExecuted),
                              &(thread->taskListMutex));
        }
    }
    else
    {	// delete the task
        periodicTask_free(ptask);
    }
    pthread_mutex_unlock(&(thread->taskListMutex));

    return 0;
}

static Periodic_Task* periodicTask_getClosest(Task_Thread* thread)
{
    // doesn't lock mutex because is called only from threadCycle
    if (thread->taskList == NULL)
    {
        return NULL;
    }
    // search for the task with minimum next scheduled time
    Periodic_Task* iterator;
    Periodic_Task* closestTask = thread->taskList;

    for (iterator = thread->taskList->next; iterator != NULL; iterator = iterator->next)
    {
        if (timespec_cmp(&(iterator->nextScheduledTime),
                         &(closestTask->nextScheduledTime)) == -1)
        {
            closestTask = iterator;
        }
    }

    return closestTask;
}

static void* threadCycle(void* arg)
{
    Task_Thread* thread = (Task_Thread*) arg;
    Periodic_Task* task;
    int waitState;

    log_debug("Periodic Task thread is started...");

    pthread_mutex_lock(&(thread->taskListMutex));
    // we have endless cycle there because thread is stopped
    // by a separate task (see stopTask) which is executed inside the loop
    while (1)
    {
        // find closest task
        task = periodicTask_getClosest(thread);
        if (task == NULL)
        {
            // wait until something is added to the taskList
            //        	log_debug("wait until something is added to the task list");
            pthread_cond_wait(&(thread->taskListUpdated), &(thread->taskListMutex));
            // start cycle from the beginning
            continue;
        }

        // wait for the time when task should be executed
        //        log_debug("wait for the time when the task should be executed");
        waitState = pthread_cond_timedwait(&(thread->taskListUpdated),
                                           &(thread->taskListMutex),
                                           &(task->nextScheduledTime));

        // check why we stopped waiting
        switch (waitState)
        {
        case 0:
            // condition _taskListUpdated is reached
            // it means that we need to check once again for the closest task
            // thus do nothing
            break;
        case ETIMEDOUT:
            // Nothing happened when we waited for the task to be executed
            // so let's execute it now
            periodicTask_execute(thread, task);
            break;
        case EINVAL:
            {
                log_error("Periodic Task thread: Next scheduled time is wrong: "
                          "%ld sec %ld nanosec. THAT IS A BIG BUG IN ptask! "
                          "THIS SHOULD NEVER HAPPEN!!!",
                          task->nextScheduledTime.tv_sec,
                          task->nextScheduledTime.tv_nsec);
                pthread_mutex_unlock(&(thread->taskListMutex));
                ptask_cancel(thread, task->id, FALSE);
                pthread_mutex_lock(&(thread->taskListMutex));
            }
            break;
        default:
            log_warning("Periodic Task thread: pthread_cond_timedwait() "
                        "returned unknown result: %d", waitState);
            // start from the beginning;
            break;
        }
    }
    // this line actually is never reached, see stopTask
    return NULL;
}

Task_Thread* ptask_init()
{
    Task_Thread* thread = (Task_Thread*) malloc(sizeof(Task_Thread));
    if (thread == NULL)
    {
        log_error("Unable to start task thread: Not enough memory");
        return NULL;
    }

    // initialize mutex and condition
    int error = pthread_mutex_init(&(thread->taskListMutex), NULL);
    if (error != 0)
    {
        log_error("Unable to start task thread: "
                  "Unable to initialize mutex (error %d).", error);
        free(thread);
        return NULL;
    }
    error = pthread_cond_init(&(thread->taskListUpdated), NULL);
    if (error != 0)
    {
        log_error("Unable to start task thread: "
                  "Unable to initialize condition (error %d).", error);
        free(thread);
        return NULL;
    }
    error = pthread_cond_init(&(thread->taskExecuted), NULL);
    if (error != 0)
    {
        log_error("Unable to start task thread: "
                  "Unable to initialize condition (error %d).", error);
        free(thread);
        return NULL;
    }

    // initialize other fields
    thread->id_gen = 1;
    thread->taskList = NULL;

    // start the thread
    error = pthread_create(&(thread->thread), NULL, &threadCycle, thread);
    if (error != 0)
    {
        log_error("Unable to start a new thread (error %d).", error);
        free(thread);
        return NULL;
    }

    return thread;
}

static void periodicTask_deleteRecursive(Periodic_Task* ptask)
{
    if (ptask == NULL)
    {
        return;
    }

    periodicTask_deleteRecursive(ptask->next);

    periodicTask_free(ptask);
}

static void stopTask(void* arg)
{
    Task_Thread* thread = (Task_Thread*) arg;
    // delete all tasks recursively
    pthread_mutex_lock(&(thread->taskListMutex));
    periodicTask_deleteRecursive(thread->taskList);
    thread->taskList = NULL;
    pthread_mutex_unlock(&(thread->taskListMutex));

    // stop the thread
    pthread_mutex_destroy(&(thread->taskListMutex));
    pthread_cond_destroy(&(thread->taskListUpdated));
    pthread_cond_destroy(&(thread->taskExecuted));
    free(thread);
    log_debug("Periodic Task thread is stopped.");
    pthread_exit(NULL);
}

int ptask_dispose(Task_Thread* thread, BOOL wait)
{
    //schedule immediate execution of the stop task
    if (thread == NULL)
    {
        return -1;
    }

    int error = ptask_schedule(thread, &stopTask, thread, 0, 1);
    if (error <= 0)
    {
        return error;
    }

    if (wait)
    {
        error = pthread_join(thread->thread, NULL);
        return error;
    }

    return 0;
}

