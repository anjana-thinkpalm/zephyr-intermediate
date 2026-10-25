
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

#define STACK_SIZE              2048
#define CONTROL_PRIORITY           7

/* Correction: maintenance runs at a lower priority than control. */
#define MAINTENANCE_PRIORITY       8

#define EVENT_PERIOD_MS          250
#define MAINTENANCE_LOAD_US    45000
#define CONTROL_DEADLINE_MS       10
#define WARNING_INTERVAL_MS     1000

struct control_event {
    uint32_t seq;
    uint32_t ready_ms;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

/* Generate periodic control events. */
static void event_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    static uint32_t seq;

    struct control_event event = {
        .seq = seq++,
        .ready_ms = k_uptime_get_32(),
    };

    /* Count only events successfully queued. */
    if (k_msgq_put(&control_queue, &event, K_NO_WAIT) != 0) {
        LOG_WRN("[TIMER] Queue full; event dropped");
        return;
    }

    k_sem_give(&maintenance_start);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* Process control events and measure response latency. */
static void control_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    uint32_t deadline_misses = 0U;
    uint32_t last_warning_ms = 0U;

    while (true) {
        struct control_event event;

        int ret = k_msgq_get(&control_queue, &event, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("[CONTROL] Queue receive failed: %d", ret);
            continue;
        }

        /* Response time: event ready to control processing start. */
        uint32_t start_ms = k_uptime_get_32();
        uint32_t latency_ms = start_ms - event.ready_ms;

        if (latency_ms > CONTROL_DEADLINE_MS) {
            deadline_misses++;

            /* Rate-limit warning logs while counting every miss. */
            if (last_warning_ms == 0U ||
                (uint32_t)(start_ms - last_warning_ms) >=
                    WARNING_INTERVAL_MS) {
                LOG_WRN("[CONTROL] Deadline miss: seq=%u "
                        "latency=%u ms deadline=%u ms total=%u",
                        event.seq,
                        latency_ms,
                        CONTROL_DEADLINE_MS,
                        deadline_misses);

                last_warning_ms = start_ms;
            }
        }

        LOG_INF("[CONTROL] seq=%u ready=%u ms start=%u ms "
                "latency=%u ms misses=%u",
                event.seq,
                event.ready_ms,
                start_ms,
                latency_ms,
                deadline_misses);
    }
}

/* Simulate maintenance work that consumes CPU time. */
static void maintenance_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        k_sem_take(&maintenance_start, K_FOREVER);

        k_busy_wait(MAINTENANCE_LOAD_US);
    }
}

K_THREAD_DEFINE(control,
                STACK_SIZE,
                control_fn,
                NULL, NULL, NULL,
                CONTROL_PRIORITY,
                0,
                0);

K_THREAD_DEFINE(maintenance,
                STACK_SIZE,
                maintenance_fn,
                NULL, NULL, NULL,
                MAINTENANCE_PRIORITY,
                0,
                0);

int main(void)
{
    LOG_INF("=== L6 Homework: Runtime Investigation ===");

    LOG_INF("Response-time deadline: %u ms",
            CONTROL_DEADLINE_MS);

    LOG_INF("Thread priorities: control=%d maintenance=%d",
            CONTROL_PRIORITY,
            MAINTENANCE_PRIORITY);

    LOG_INF("Use kernel shell and CTF tracing to inspect scheduling");

    /* First event after 500 ms, then every 250 ms. */
    k_timer_start(&event_timer,
                  K_MSEC(500),
                  K_MSEC(EVENT_PERIOD_MS));

    return 0;
}
