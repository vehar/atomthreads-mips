/*
 * Copyright (c) 2010, Kelvin Lawson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. No personal names or organizations' names associated with the
 *    Atomthreads project may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ATOMTHREADS PROJECT AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "atom.h"
#include "atomqueue.h"
#include "atomtests.h"


/* Test queue size */
#define QUEUE_ENTRIES       16


/* Test OS objects */
static ATOM_QUEUE queue1;
static ATOM_TCB tcb1, tcb2;
static uint8_t test1_thread_stack[TEST_THREAD_STACK_SIZE];
static uint8_t test2_thread_stack[TEST_THREAD_STACK_SIZE];
static uint8_t queue1_storage[QUEUE_ENTRIES];


/* Test result tracking */
static volatile int g_result;


/* Forward declarations */
static void test1_thread_func (uint32_t data);
static void test2_thread_func (uint32_t data);


/**
 * \b test_start
 *
 * Start queue test.
 *
 * This test exercises queue deletion, waking threads blocking on a queue
 * in atomQueueGet() if the queue is deleted.
 *
 * Deletion wakeups are tested twice: once for a thread which is blocking
 * in atomQueueGet() with a timeout and once for a thread which is
 * blocking in atomQueueGet() with no timeout.
 *
 * Deletion of threads blocking in atomQueuePut() are tested in queue3.c.
 *
 * @retval Number of failures
 */
uint32_t test_start (void)
{
    int failures;

    /* Default to zero failures */
    failures = 0;

    /* Test wakeup of threads on queue deletion (thread blocking with no timeout) */
    g_result = 0;
    if (atomQueueCreate (&queue1, &queue1_storage[0], sizeof(uint8_t), QUEUE_ENTRIES) != ATOM_OK)
    {
        ATOMLOG (_STR("Error creating test queue\n"));
        failures++;
    }

    /* Create a test thread that will block because the queue is empty */
    else if (atomThreadCreate(&tcb1, TEST_THREAD_PRIO, test1_thread_func, 0,
              &test1_thread_stack[TEST_THREAD_STACK_SIZE - 1]) != ATOM_OK)
    {
        /* Fail */
        ATOMLOG (_STR("Error creating test thread 1\n"));
        failures++;
    }
    else
    {

        /*
         * We have created an empty queue. We want to see that the other
         * thread is woken up if its queue is deleted. This is indicated
         * through g_result being set.
         */

        /* Wait for the other thread to start blocking on queue1 */
        if (atomTimerDelay(SYSTEM_TICKS_PER_SEC) != ATOM_OK)
        {
            ATOMLOG (_STR("Failed timer delay\n"));
            failures++;
        }
        else
        {
            /* The other thread will be blocking on queue1 now, delete queue1 */
            if (atomQueueDelete(&queue1) != ATOM_OK)
            {
                ATOMLOG (_STR("Failed queue1 delete\n"));
                failures++;
            }
            else
            {
                /* Queue1 deleted. The thread should now wake up and set g_result. */
                atomTimerDelay (SYSTEM_TICKS_PER_SEC);
                if (g_result == 0)
                {
                    ATOMLOG (_STR("Notify fail\n"));
                    failures++;
                }
                else
                {
                    /* Success */
                }
            }
        }
    }

    /* Test wakeup of threads on semaphore deletion (thread blocking with timeout) */
    g_result = 0;
    if (atomQueueCreate (&queue1, &queue1_storage[0], sizeof(uint8_t), QUEUE_ENTRIES) != ATOM_OK)
    {
        ATOMLOG (_STR("Error creating test queue\n"));
        failures++;
    }

    /* Create a test thread that will block because the queue is empty */
    else if (atomThreadCreate(&tcb2, TEST_THREAD_PRIO, test2_thread_func, 0,
              &test2_thread_stack[TEST_THREAD_STACK_SIZE - 1]) != ATOM_OK)
    {
        /* Fail */
        ATOMLOG (_STR("Error creating test thread 2\n"));
        failures++;
    }
    else
    {

        /*
         * We have created an empty queue. We want to see that the other
         * thread is woken up if its queue is deleted. This is indicated
         * through g_result being set.
         */

        /* Wait for the other thread to start blocking on queue1 */
        if (atomTimerDelay(SYSTEM_TICKS_PER_SEC) != ATOM_OK)
        {
            ATOMLOG (_STR("Failed timer delay\n"));
            failures++;
        }
        else
        {
            /* The other thread will be blocking on queue1 now, delete queue1 */
            if (atomQueueDelete(&queue1) != ATOM_OK)
            {
                ATOMLOG (_STR("Failed queue1 delete\n"));
                failures++;
            }
            else
            {
                /* Queue1 deleted. The thread should now wake up and set g_result. */
                atomTimerDelay (SYSTEM_TICKS_PER_SEC);
                if (g_result == 0)
                {
                    ATOMLOG (_STR("Notify fail\n"));
                    failures++;
                }
                else
                {
                    /* Success */
                }
            }
        }
    }

    /* Log final status */
    if (failures == 0)
    {
        ATOMLOG (_STR("Pass\n"));
    }
    else
    {
        ATOMLOG (_STR("Fail(%d)\n"), failures);
    }

    /* Quit */
    return failures;
}


/**
 * \b test1_thread_func
 *
 * Entry point for test thread 1.
 *
 * @param[in] data Unused (optional thread entry parameter)
 *
 * @return None
 */
static void test1_thread_func (uint32_t data)
{
    uint8_t status, msg;

    /*
     * Wait on queue1 with no timeout. We are expecting to be woken up
     * by the main thread while blocking.
     */
    status = atomQueueGet(&queue1, 0, &msg);
    if (status != ATOM_ERR_DELETED)
    {
        ATOMLOG (_STR("Test1 thread woke without deletion (%d)\n"), status);
    }
    else
    {
        /* We were woken due to deletion as expected, set g_result to notify success */
        g_result = 1;
    }

    /* Wait forever */
    while (1)
    {
        atomTimerDelay (SYSTEM_TICKS_PER_SEC);
    }
}


/**
 * \b test2_thread_func
 *
 * Entry point for test thread 2.
 *
 * @param[in] data Unused (optional thread entry parameter)
 *
 * @return None
 */
static void test2_thread_func (uint32_t data)
{
    uint8_t status, msg;

    /*
     * Wait on queue1 with timeout. We are expecting to be woken up
     * by the main thread while blocking.
     */
    status = atomQueueGet(&queue1, (5 * SYSTEM_TICKS_PER_SEC), &msg);
    if (status != ATOM_ERR_DELETED)
    {
        ATOMLOG (_STR("Test2 thread woke without deletion (%d)\n"), status);
    }
    else
    {
        /* We were woken due to deletion as expected, set g_result to notify success */
        g_result = 1;
    }

    /* Wait forever */
    while (1)
    {
        atomTimerDelay (SYSTEM_TICKS_PER_SEC);
    }
}