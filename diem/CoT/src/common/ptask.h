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
 * Periodic Task - tool for asynchronous task execution.
 *
 * Periodic Task utility can be used to schedule some function(s) to
 * be invoked periodically in a separate thread. A function can scheduled to be
 * invoked either defined number of times or indefinite (until it is canceled).
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef PTASK_H_
#define PTASK_H_

#include "bool.h"

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
 * @param wait   If #TRUE than the method will block and wait until specified
 *               thread is really disposed. Otherwise, method will only schedule
 *               asynchronous disposing of the thread.
 * @return @a 0 on success, negative error code otherwise.
 */
int ptask_dispose(Task_Thread* thread, BOOL wait);

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
 * @return @li >0 - ID of the scheduled task.
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
 *                     task is executed until #ptask_cancel() is called with
 *                     corresponding task ID.
 * @param add Defines whether time provided in @a period argument will be used
 *            as new execution period, or will be added to the current one.
 * @note When @a add is set to #TRUE, @a period will be also added to the next
 *       execution time, but when @a add is #FALSE the next execution will be
 *       (current time + @a period).
 * @return @a 0 on success, negative error code otherwise.
 */
int ptask_reschedule(Task_Thread* thread,
                     int taskId,
                     long period,
                     int executeTimes,
                     BOOL add);

/**
 * Checks whether the task with provided id is scheduled for execution in the
 * thread.
 *
 * @param thread Thread where the task should be searched for.
 * @param taskId Task id which is searched for.
 * @return #TRUE if the task with specified @a taskId is scheduled,
 *         #FALSE otherwise.
 */
BOOL ptask_isScheduled(Task_Thread* thread, int taskId);

/**
 * Resets time until the next execution of the specified task.
 * The next execution time will be current time + @a period provided when the
 * task was scheduled. If the @a period needs to be changed than use
 * #ptask_reschedule() instead.
 *
 * @param thread Thread where the task is scheduled.
 * @param taskId Id of the task whose execution time should be reset.
 * @return @a 0 on success, negative error code otherwise.
 */
int ptask_reset(Task_Thread* thread, int taskId);

/**
 * Removes task from the scheduled list.
 *
 * @param thread Thread in which task is scheduled.
 * @param taskId ID of the task to be removed.
 * @param wait When task is being executed it can be canceled only after
 *             execution is completed. This parameter defines whether the
 *             function should wait until the task is really canceled, or it can
 *             just mark the task as canceled, which guarantees that the task
 *             will be removed as soon as the current execution is completed.
 *             In case when this function is called while the task is not
 *             executed @a wait argument makes no difference.
 * @return @li @a 0 on success;
 *         @li @a -1 if task with provided ID is not found.
 */
int ptask_cancel(Task_Thread* thread, int taskId, BOOL wait);

#endif /* PTASK_H_ */
