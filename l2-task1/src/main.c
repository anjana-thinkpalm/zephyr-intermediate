#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE       1024
#define THREAD_PRIORITY  5
#define NUM_ITERATIONS   10000

static int shared_counter;

/* Mutex to protect the shared counter */
static struct k_mutex counter_mutex;

/* Thread stacks */
K_THREAD_STACK_DEFINE(race_stack1, STACK_SIZE);
K_THREAD_STACK_DEFINE(race_stack2, STACK_SIZE);
K_THREAD_STACK_DEFINE(mutex_stack1, STACK_SIZE);
K_THREAD_STACK_DEFINE(mutex_stack2, STACK_SIZE);

/* Thread control blocks */
static struct k_thread race_thread1;
static struct k_thread race_thread2;
static struct k_thread mutex_thread1;
static struct k_thread mutex_thread2;

/*
 * Race-condition test.
 *
 * Both threads perform a read-modify-write operation
 * without protecting the shared variable.
 */
static void race_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int temp = shared_counter;

        /*
         * Give the other thread an opportunity to run
         * between the read and write operations.
         */
        k_busy_wait(1);

        shared_counter = temp + 1;
    }
}

/*
 * Mutex-protected test.
 */
static void mutex_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        shared_counter++;

        k_mutex_unlock(&counter_mutex);
    }
}

int main(void)
{
    const int expected = NUM_ITERATIONS * 2;

    printk("\n==============================\n");
    printk("L2 Task 1 - Race Condition\n");
    printk("==============================\n");

    /*
     * ------------------------------------
     * Test 1: Race condition
     * ------------------------------------
     */
    printk("\n[TEST 1] Without mutex\n");

    shared_counter = 0;

    k_thread_create(&race_thread1,
                    race_stack1,
                    STACK_SIZE,
                    race_thread_entry,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    k_thread_create(&race_thread2,
                    race_stack2,
                    STACK_SIZE,
                    race_thread_entry,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    k_thread_join(&race_thread1, K_FOREVER);
    k_thread_join(&race_thread2, K_FOREVER);

    printk("Expected counter: %d\n", expected);
    printk("Actual counter:   %d\n", shared_counter);

    if (shared_counter != expected) {
        printk("Race condition observed!\n");
    } else {
        printk("Race condition was not observed.\n");
    }

    /*
     * ------------------------------------
     * Test 2: Mutex
     * ------------------------------------
     */
    printk("\n[TEST 2] With mutex\n");

    shared_counter = 0;

    k_mutex_init(&counter_mutex);

    k_thread_create(&mutex_thread1,
                    mutex_stack1,
                    STACK_SIZE,
                    mutex_thread_entry,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    k_thread_create(&mutex_thread2,
                    mutex_stack2,
                    STACK_SIZE,
                    mutex_thread_entry,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    k_thread_join(&mutex_thread1, K_FOREVER);
    k_thread_join(&mutex_thread2, K_FOREVER);

    printk("Expected counter: %d\n", expected);
    printk("Actual counter:   %d\n", shared_counter);

    if (shared_counter == expected) {
        printk("Mutex fixed the race condition!\n");
    } else {
        printk("Unexpected result.\n");
    }

    printk("\nTask completed.\n");

    return 0;
}