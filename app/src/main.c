#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/*
 * Thread priorities:
 * Lower number = higher priority.
 *
 * t_high: priority 3
 * t_med : priority 5
 * t_low : priority 7
 *
 * Cooperative thread:
 * priority -1
 */

#define STACK_SIZE 1024

#define LOW_PRIORITY    7
#define MED_PRIORITY    5
#define HIGH_PRIORITY   3
#define COOP_PRIORITY  -1

#define LOW_SLEEP_MS    300
#define MED_SLEEP_MS    200
#define HIGH_SLEEP_MS   100

#define COOP_ITERATIONS 5

K_THREAD_STACK_DEFINE(low_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(med_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(high_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(coop_stack, STACK_SIZE);

struct k_thread low_thread;
struct k_thread med_thread;
struct k_thread high_thread;
struct k_thread coop_thread;

static void t_low_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("T_LOW running\n");
		k_sleep(K_MSEC(LOW_SLEEP_MS));
	}
}

static void t_med_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("T_MED running\n");
		k_sleep(K_MSEC(MED_SLEEP_MS));
	}
}

static void t_high_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("T_HIGH running\n");
		k_sleep(K_MSEC(HIGH_SLEEP_MS));
	}
}

static void t_coop_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		for (int i = 0; i < COOP_ITERATIONS; i++) {
			volatile uint32_t busy = 0;

			for (uint32_t j = 0; j < 100000; j++) {
				busy += j;
			}

			ARG_UNUSED(busy);
		}

		printk("T_COOP yielding\n");
		k_yield();
	}
}

int main(void)
{
	printk("L1 Task 1: Thread scheduling demo\n");

	k_thread_create(&low_thread,
			low_stack,
			K_THREAD_STACK_SIZEOF(low_stack),
			t_low_fn,
			NULL, NULL, NULL,
			LOW_PRIORITY,
			0,
			K_NO_WAIT);

	k_thread_create(&med_thread,
			med_stack,
			K_THREAD_STACK_SIZEOF(med_stack),
			t_med_fn,
			NULL, NULL, NULL,
			MED_PRIORITY,
			0,
			K_NO_WAIT);

	k_thread_create(&high_thread,
			high_stack,
			K_THREAD_STACK_SIZEOF(high_stack),
			t_high_fn,
			NULL, NULL, NULL,
			HIGH_PRIORITY,
			0,
			K_NO_WAIT);

	k_thread_create(&coop_thread,
			coop_stack,
			K_THREAD_STACK_SIZEOF(coop_stack),
			t_coop_fn,
			NULL, NULL, NULL,
			COOP_PRIORITY,
			0,
			K_NO_WAIT);

	return 0;
}