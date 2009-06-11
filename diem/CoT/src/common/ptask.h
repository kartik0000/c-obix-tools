/** @file
 *
 * Contains interface definition of the Periodic Task.
 *
 * Periodic Task utility can be used to schedule some function to
 * be invoked in a separate thread periodically after defined delay.
 * A function can scheduled to be invoked either defined number of times
 * (starting from 1) or until it is canceled.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef PTASK_H_
#define PTASK_H_

///@todo Create separate header for BOOL
#include "ixml_ext.h"

/**
 * Specifies that the task should be executed indefinite number of times
 * (until #ptask_cancel() is called).
 */
#define EXECUTE_INDEFINITE -1

/**
 * Prototype of the function which can be scheduled.
 *
 * @param arg Argument which is passed to the function when it is invoked.
 */
typedef void (*periodic_task)(void* arg);

/**
 * Represents a separate thread which can be used to schedule tasks.
 */
typedef struct _Task_Thread Task_Thread;

/**
 * Creates new instance of #Task_Thread.
 *
 * @return Pointer to the new instance of #Task_Thread, or @a NULL if some
 *         error occurred.
 */
Task_Thread* ptask_init();

/**
 * Releases resources allocated for the provided #Task_Thread instance.
 * All scheduled tasks are canceled.
 *
 * @param thread Pointer to the #Task_Thread to be freed.
 * @return @a 0 on success, negative error code otherwise.
 */
int ptask_dispose(Task_Thread* thread);

/**
 * Schedules new task for execution.
 *
 * @param thread Thread in which the task will be executed.
 * @param task Task which should be scheduled.
 * @param arg Argument which will be passed to the task function each time when
 *            it is invoked.
 * @param period Time interval in milliseconds, which defines how often the
 *               task will be executed.
 * @param executeTimes Defines how many times (min. 1) the task should be
 *                     executed. If #EXECUTE_INDEFINITE is provided than the
 *                     task is executed until #ptask_cancel() with
 *                     corresponding task ID is called.
 * @return @li >=0 - ID of the scheduled task. Can be used to cancel the task.
 *         @li <0 - Error code.
 */
int ptask_schedule(Task_Thread* thread,
                   periodic_task task,
                   void* arg,
                   long period,
                   int executeTimes);

/**
 * Sets new execution period for the specified task.
 *
 * @param thread Thread in which the task is scheduled.
 * @param taskId Id of the scheduled task.
 * @param period New time interval in milliseconds (or time which will be added
 *               to the current task period).
 * @param executeTimes Defines how many times (min. 1) the task should be
 *                     executed. If #EXECUTE_INDEFINITE is provided than the
 *                     task is executed until #ptask_cancel() with
 *                     corresponding task ID is called.
 * @param add Defines whether time provided in @a period argument will be used
 *            as new execution period, or will be added to the current one.
 */
int ptask_reschedule(Task_Thread* thread,
                     int taskId,
                     long period,
                     int executeTimes,
                     BOOL add);

int ptask_reset(Task_Thread* thread, int taskId);

/**
 * Removes task from the scheduled list.
 *
 * @param thread Thread in which task is scheduled.
 * @param id ID of the task to be removed.
 * @return @li 0 on success;
 *         @li -1 if task with provided ID is not found.
 */
int ptask_cancel(Task_Thread* thread, int taskId);

#endif /* PTASK_H_ */
