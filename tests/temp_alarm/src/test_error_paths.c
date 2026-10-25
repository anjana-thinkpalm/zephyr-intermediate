/*
 * temp_alarm - Error-path unit tests (homework skeleton)
 *
 * Task 2: fill in the stubs according to TEST_SPEC.md. Each stub calls
 * ztest_test_skip(), so the binary builds and runs cleanly until the test is
 * implemented.
 *
 * Run:
 *   west twister -T tests/temp_alarm -p native_sim
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/* Same before() hook as in test_basic.c: a clean fake and an initialised module. */
static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	sensor_fake_reset();
	sensor_fake_init();

	zassume_ok(temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
		   "precondition: temp_alarm_init must succeed");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_sensor_errors
 *
 * Failures from the sensor driver, injected through the fakes.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_sensor_errors, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_sensor_errors, test_fetch_error_propagates)
{
	struct temp_alarm_status status;

	fake_sensor_sample_fetch_fake.return_val = -EIO;

	zassert_equal(temp_alarm_check(), -EIO, "check must return the fetch error");
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d",
		      status.last_error);

	zassert_equal(fake_sensor_channel_get_fake.call_count, 0U,
		      "channel_get must not be called after a failed fetch");
}

ZTEST(temp_alarm_sensor_errors, test_channel_get_error_propagates)
{
	struct temp_alarm_status status;

	/* Overrides the custom_fake that sensor_fake_init() installed. */
	fake_sensor_channel_get_fake.custom_fake = NULL;
	fake_sensor_channel_get_fake.return_val = -EIO;

	zassert_equal(temp_alarm_check(), -EIO, "check must return the channel_get error");
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d",
		      status.last_error);

	zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1U,
		      "sample must still be fetched exactly once");
}

ZTEST(temp_alarm_sensor_errors, test_fail_then_recover)
{
	struct temp_alarm_status status;

	int fetch_results[] = {-EIO, 0};

	SET_RETURN_SEQ(fake_sensor_sample_fetch, fetch_results, ARRAY_SIZE(fetch_results));
	sensor_fake_set_temperature(20);

	zassert_equal(temp_alarm_check(), -EIO, "first check must fail with -EIO");
	zassert_ok(temp_alarm_check(), "second check must succeed");

	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_equal(status.last_temp, 20, "last_temp must be 20, got %d", status.last_temp);
	zassert_false(status.is_alarming, "20 C is below the threshold");
	zassert_equal(status.last_error, -EIO, "last_error must be kept until reset, got %d",
		      status.last_error);
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_invalid_input
 *
 * Arguments the public API must reject.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_invalid_input, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_invalid_input, test_get_status_null)
{
	zassert_equal(temp_alarm_get_status(NULL), -EINVAL, "NULL status must be rejected");
}

ZTEST(temp_alarm_invalid_input, test_init_without_device)
{
	zassert_equal(temp_alarm_init(NULL, DEFAULT_THRESHOLD), -ENODEV,
		      "a NULL device must be rejected with -ENODEV");
}

ZTEST(temp_alarm_invalid_input, test_init_threshold_out_of_range)
{
	zassert_equal(temp_alarm_init(&fake_sensor_dev, -1), -EINVAL,
		      "threshold -1 must be rejected");
	zassert_equal(temp_alarm_init(&fake_sensor_dev, 101), -EINVAL,
		      "threshold 101 must be rejected");
}

ZTEST(temp_alarm_invalid_input, test_set_threshold_out_of_range)
{
	struct temp_alarm_status status;

	zassert_equal(temp_alarm_set_threshold(-1), -EINVAL, "threshold -1 must be rejected");
	zassert_equal(temp_alarm_set_threshold(101), -EINVAL, "threshold 101 must be rejected");

	/* The old threshold (30 C) must still be in force. */
	sensor_fake_set_temperature(30);
	zassert_ok(temp_alarm_check(), "check must succeed");
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_false(status.is_alarming, "30 C is not above a threshold of 30 C");

	sensor_fake_set_temperature(31);
	zassert_ok(temp_alarm_check(), "check must succeed");
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_true(status.is_alarming, "31 C is above a threshold of 30 C");
}
