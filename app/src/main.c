/*
 * Lecture 4 - Homework
 *
 * GOAL:
 * Build a small event-driven system using Zephyr zbus.
 *
 * Architecture:
 *
 *              Sensor simulator
 *                     |
 *                     | publish
 *                     v
 *              +-------------+
 *              | sensor_chan |
 *              +-------------+
 *                  /       \
 *                 /         \
 *                v           v
 *          Listener       Subscriber
 *          fast display   slow logging
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(l4_homework, LOG_LEVEL_INF);

#define STACK_SIZE 1024
#define SENSOR_PERIOD_MS 100
#define SENSOR_COUNT 10
#define SUBSCRIBER_DELAY_MS 300

/*
 * Message transferred through zbus.
 */
struct sensor_data {
    int sample;
    int temperature;
};

/*
 * Forward declaration of the channel.
 */
ZBUS_CHAN_DECLARE(sensor_chan);

/*
 * ---------------------------------------------------------
 * Listener
 * ---------------------------------------------------------
 *
 * A listener is called whenever sensor_chan is published.
 *
 * This is the fast display path.
 */
static void sensor_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_data *data;

    if (chan != &sensor_chan) {
        return;
    }

    /*
     * Listener callbacks can directly access the channel
     * message because zbus invokes the listener with the
     * channel locked.
     */
    data = zbus_chan_const_msg(chan);

    LOG_INF("[LISTENER] sample=%d temperature=%d",
            data->sample,
            data->temperature);
}

ZBUS_LISTENER_DEFINE(sensor_listener, sensor_listener_cb);

/*
 * ---------------------------------------------------------
 * Subscriber
 * ---------------------------------------------------------
 *
 * The subscriber waits for notifications from zbus.
 *
 * It represents a slower logging consumer.
 */
ZBUS_SUBSCRIBER_DEFINE(sensor_subscriber, 4);

/*
 * ---------------------------------------------------------
 * Sensor channel
 * ---------------------------------------------------------
 *
 * One channel contains sensor_data.
 *
 * Both the listener and subscriber observe this channel.
 */
ZBUS_CHAN_DEFINE(sensor_chan,
                 struct sensor_data,
                 NULL,
                 NULL,
                 ZBUS_OBSERVERS(sensor_listener,
                                sensor_subscriber),
                 ZBUS_MSG_INIT(.sample = 0,
                               .temperature = 0));

/*
 * ---------------------------------------------------------
 * Sensor simulator
 * ---------------------------------------------------------
 *
 * Generates one sensor sample every 100 ms and publishes
 * it to the zbus channel.
 */
static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 1; i <= SENSOR_COUNT; i++) {
        struct sensor_data data = {
            .sample = i,
            .temperature = 20 + i,
        };

        k_msleep(SENSOR_PERIOD_MS);

        LOG_INF("[SENSOR] publishing sample=%d temperature=%d",
                data.sample,
                data.temperature);

        int ret = zbus_chan_pub(&sensor_chan,
                                &data,
                                K_MSEC(100));

        if (ret != 0) {
            LOG_ERR("[SENSOR] publish failed: %d", ret);
        }
    }

    LOG_INF("[SENSOR] all samples published");
}

/*
 * Sensor simulator thread.
 */
K_THREAD_DEFINE(sensor_thread,
                STACK_SIZE,
                sensor_sim_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);

/*
 * ---------------------------------------------------------
 * Subscriber thread
 * ---------------------------------------------------------
 *
 * Waits for zbus notifications.
 *
 * After receiving a channel notification, it reads the
 * latest data from the channel.
 */
static void subscriber_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct zbus_channel *chan;

    while (1) {
        /*
         * Wait until one of the observed channels changes.
         */
        int ret = zbus_sub_wait(&sensor_subscriber,
                                &chan,
                                K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[SUBSCRIBER] wait failed: %d", ret);
            continue;
        }

        /*
         * Make sure this notification belongs to our channel.
         */
        if (chan == &sensor_chan) {
            struct sensor_data data;

            /*
             * Read the latest value from the channel.
             */
            ret = zbus_chan_read(chan,
                                 &data,
                                 K_MSEC(100));

            if (ret != 0) {
                LOG_ERR("[SUBSCRIBER] read failed: %d", ret);
                continue;
            }

            LOG_INF("[SUBSCRIBER] logging sample=%d temperature=%d",
                    data.sample,
                    data.temperature);

            /*
             * Simulate slower logging work.
             */
            k_msleep(SUBSCRIBER_DELAY_MS);
        }
    }
}

/*
 * Subscriber thread.
 */
K_THREAD_DEFINE(subscriber_thread,
                STACK_SIZE,
                subscriber_fn,
                NULL,
                NULL,
                NULL,
                7,
                0,
                0);

/*
 * ---------------------------------------------------------
 * Main
 * ---------------------------------------------------------
 */
int main(void)
{
    LOG_INF("================================");
    LOG_INF("L4 Homework - zbus");
    LOG_INF("================================");

    LOG_INF("Sensor period      : %d ms",
            SENSOR_PERIOD_MS);

    LOG_INF("Expected samples   : %d",
            SENSOR_COUNT);

    LOG_INF("Listener           : fast display");

    LOG_INF("Subscriber         : slow logging");

    /*
     * Give the sensor and subscriber threads enough time
     * to execute.
     */
    k_msleep((SENSOR_COUNT * SENSOR_PERIOD_MS) +
             2000);

    LOG_INF("================================");
    LOG_INF("L4 zbus test complete");
    LOG_INF("================================");

    return 0;
}