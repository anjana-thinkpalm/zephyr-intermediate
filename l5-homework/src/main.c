
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

#include <errno.h>
#include <stdint.h>

LOG_MODULE_REGISTER(l5_homework, LOG_LEVEL_INF);

#define QUEUE_CAPACITY          10
#define PRODUCER_INTERVAL_MS    400
#define CONSUMER_INTERVAL_MS    200
#define HEALTH_INTERVAL_MS      500

#define WATCHDOG_TIMEOUT_MS     3000
#define STUCK_SLEEP_MS          12000
#define STUCK_EVERY_N_ITEMS     8
#define WARNING_PERCENT         75

struct message {
    uint32_t sequence;
};

K_MSGQ_DEFINE(message_queue,
              sizeof(struct message),
              QUEUE_CAPACITY,
              4);

static int consumer_wdt_channel = -1;

static atomic_t produced_count;
static atomic_t consumed_count;
static atomic_t dropped_count;

static void consumer_watchdog_callback(int channel_id,
                                        void *user_data)
{
    ARG_UNUSED(user_data);

    LOG_ERR("WATCHDOG FIRED: consumer timed out, channel %d",
            channel_id);
}

static void producer_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    struct message msg = {0};

    while (1) {
        msg.sequence = (uint32_t)atomic_inc(&produced_count) + 1U;

        int ret = k_msgq_put(&message_queue, &msg, K_NO_WAIT);

        if (ret == 0) {
            LOG_INF("Produced item %u", msg.sequence);
        } else if (ret == -ENOMSG) {
            int drops = atomic_inc(&dropped_count) + 1;

            LOG_WRN("Queue full; dropped item %u (drops: %d)",
                    msg.sequence, drops);
        } else {
            LOG_ERR("Queue put failed: %d", ret);
        }

        k_msleep(PRODUCER_INTERVAL_MS);
    }
}

static void consumer_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    struct message msg;

    while (1) {
        int ret = k_msgq_get(&message_queue, &msg, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("Queue get failed: %d", ret);
            continue;
        }

        /*
         * Deliberately simulate a stuck consumer.
         * The watchdog is not fed during this sleep.
         */
        if ((msg.sequence % STUCK_EVERY_N_ITEMS) == 0U) {
            LOG_WRN("Simulating stuck consumer at item %u",
                    msg.sequence);

            k_msleep(STUCK_SLEEP_MS);
        }

        LOG_INF("Consumed item %u", msg.sequence);
        atomic_inc(&consumed_count);

        if (consumer_wdt_channel >= 0) {
            ret = task_wdt_feed(consumer_wdt_channel);

            if (ret != 0) {
                LOG_ERR("Watchdog feed failed: %d", ret);
            }
        }

        k_msleep(CONSUMER_INTERVAL_MS);
    }
}

static void health_check_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    while (1) {
        uint32_t used = k_msgq_num_used_get(&message_queue);
        uint32_t percent = (used * 100U) / QUEUE_CAPACITY;

        LOG_INF("Queue: %u/%u (%u%%), produced=%d, consumed=%d",
                used,
                QUEUE_CAPACITY,
                percent,
                (int)atomic_get(&produced_count),
                (int)atomic_get(&consumed_count));

        if (percent >= WARNING_PERCENT) {
            LOG_WRN("HIGH QUEUE UTILIZATION: %u%% (threshold: %u%%)",
                    percent, WARNING_PERCENT);
        }

        k_msleep(HEALTH_INTERVAL_MS);
    }
}

K_THREAD_STACK_DEFINE(producer_stack, 2048);
K_THREAD_STACK_DEFINE(consumer_stack, 2048);
K_THREAD_STACK_DEFINE(health_stack, 2048);

static struct k_thread producer_thread_data;
static struct k_thread consumer_thread_data;
static struct k_thread health_thread_data;

int main(void)
{
    int ret;

    LOG_INF("Starting L5 producer-consumer reliability test");

    ret = task_wdt_init(NULL);
    if (ret != 0) {
        LOG_ERR("Task watchdog initialization failed: %d", ret);
        return 0;
    }

    consumer_wdt_channel =
        task_wdt_add(WATCHDOG_TIMEOUT_MS,
                     consumer_watchdog_callback,
                     NULL);

    if (consumer_wdt_channel < 0) {
        LOG_ERR("Could not add watchdog channel: %d",
                consumer_wdt_channel);
        return 0;
    }

    k_thread_create(&producer_thread_data,
                    producer_stack,
                    K_THREAD_STACK_SIZEOF(producer_stack),
                    producer_thread,
                    NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);

    k_thread_create(&consumer_thread_data,
                    consumer_stack,
                    K_THREAD_STACK_SIZEOF(consumer_stack),
                    consumer_thread,
                    NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);

    k_thread_create(&health_thread_data,
                    health_stack,
                    K_THREAD_STACK_SIZEOF(health_stack),
                    health_check_thread,
                    NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);

    LOG_INF("Producer, consumer and health-check threads started");

    return 0;
}
