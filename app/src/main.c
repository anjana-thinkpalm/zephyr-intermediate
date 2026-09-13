#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5
#define INCREMENTS 100000

static int shared_counter = 0;

K_MUTEX_DEFINE(counter_mutex);

K_THREAD_STACK_DEFINE(thread1_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread2_stack, STACK_SIZE);

static struct k_thread thread1_data;
static struct k_thread thread2_data;

static void thread1_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (int i = 0; i < INCREMENTS; i++) {
		k_mutex_lock(&counter_mutex, K_FOREVER);

		shared_counter++;

		k_mutex_unlock(&counter_mutex);
	}

	printk("Thread 1 finished\n");
}

static void thread2_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (int i = 0; i < INCREMENTS; i++) {
		k_mutex_lock(&counter_mutex, K_FOREVER);

		shared_counter++;

		k_mutex_unlock(&counter_mutex);
	}

	printk("Thread 2 finished\n");
}

int main(void)
{
	printk("L2 Task 1: Race condition and mutex\n");
	printk("Expected counter: %d\n", INCREMENTS * 2);

	k_thread_create(&thread1_data,
			thread1_stack,
			K_THREAD_STACK_SIZEOF(thread1_stack),
			thread1_fn,
			NULL, NULL, NULL,
			THREAD_PRIORITY,
			0,
			K_NO_WAIT);

	k_thread_create(&thread2_data,
			thread2_stack,
			K_THREAD_STACK_SIZEOF(thread2_stack),
			thread2_fn,
			NULL, NULL, NULL,
			THREAD_PRIORITY,
			0,
			K_NO_WAIT);

	k_thread_join(&thread1_data, K_FOREVER);
	k_thread_join(&thread2_data, K_FOREVER);

	printk("Final counter: %d\n", shared_counter);

	return 0;
}