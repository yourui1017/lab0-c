/** dude, is my code constant time?
 *
 * This file measures the execution time of a given function many times with
 * different inputs and performs a Welch's t-test to determine if the function
 * runs in constant time or not. This is essentially leakage detection, and
 * not a timing attack.
 *
 * Notes:
 *
 *  - the execution time distribution tends to be skewed towards large
 *    timings, leading to a fat right tail. Most executions take little time,
 *    some of them take a lot. We try to speed up the test process by
 *    throwing away those measurements with large cycle count. (For example,
 *    those measurements could correspond to the execution being interrupted
 *    by the OS.) Setting a threshold value for this is not obvious; we just
 *    keep the x% percent fastest timings, and repeat for several values of x.
 *
 *  - the previous observation is highly heuristic. We also keep the uncropped
 *    measurement time and do a t-test on that.
 *
 *  - we also test for unequal variances (second order test), but this is
 *    probably redundant since we're doing as well a t-test on cropped
 *    measurements (non-linear transform)
 *
 *  - as long as any of the different test fails, the code will be deemed
 *    variable time.
 */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../console.h"
#include "../random.h"

#include "constant.h"
#include "fixture.h"
#include "ttest.h"

#define ENOUGH_MEASURE 10000
#define NUMBER_PERCENTILES 100
#define DUDECT_TESTS (1 + NUMBER_PERCENTILES + 1)
#define TEST_TRIES 10

static t_context_t *t[DUDECT_TESTS];

/* threshold values for Welch's t-test */
enum {
    t_threshold_bananas = 500, /* Test failed with overwhelming probability */
    t_threshold_moderate = 10, /* Test failed */
};

static void __attribute__((noreturn)) die(void)
{
    exit(111);
}

static void differentiate(int64_t *exec_times,
                          const int64_t *before_ticks,
                          const int64_t *after_ticks)
{
    for (size_t i = 0; i < N_MEASURES; i++)
        exec_times[i] = after_ticks[i] - before_ticks[i];
}

static int cmp(const int64_t *a, const int64_t *b)
{
    return (int) (*a - *b);
}

static int64_t percentile(int64_t *a_sorted, double which, size_t size)
{
    size_t array_position = (size_t) ((double) size * (double) which);
    assert(array_position < size);
    return a_sorted[array_position];
}

static void prepare_percentiles(int64_t *exec_times, int64_t *percentiles)
{
    qsort(exec_times, N_MEASURES, sizeof(int64_t),
          (int (*)(const void *, const void *)) cmp);
    for (size_t i = 0; i < NUMBER_PERCENTILES; i++) {
        percentiles[i] = percentile(
            exec_times,
            1 - (pow(0.5, 10 * (double) (i + 1) / NUMBER_PERCENTILES)),
            N_MEASURES);
    }
}

static void update_statistics(const int64_t *exec_times,
                              uint8_t *classes,
                              int64_t *percentiles)
{
    for (size_t i = 10; i < N_MEASURES - 1; i++) {
        int64_t difference = exec_times[i];
        /* CPU cycle counter overflowed or dropped measurement */
        if (difference <= 0)
            continue;

        /* do a t-test on the execution time */
        t_push(t[0], difference, classes[i]);

        // t-test on cropped execution times, for several cropping thresholds.
        for (size_t crop_index = 0; crop_index < NUMBER_PERCENTILES;
             crop_index++) {
            if (difference < percentiles[crop_index]) {
                t_push(t[crop_index + 1], difference, classes[i]);
            }
        }
    }
}

static bool report(void)
{
    double max_t = 0;
    int max_count = 0;
    for (int i = 0; i < DUDECT_TESTS; i++) {
        if (max_t < fabs(t_compute(t[i]))) {
            max_t = fabs(t_compute(t[i]));
            max_count = i;
        }
    }
    double number_traces_max_t = t[max_count]->n[0] + t[max_count]->n[1];
    double max_tau = max_t / sqrt(number_traces_max_t);

    printf("\033[A\033[2K");
    printf("meas: %7.2lf M, ", (number_traces_max_t / 1e6));
    if (number_traces_max_t < ENOUGH_MEASURE) {
        printf("not enough measurements (%.0f still to go).\n",
               ENOUGH_MEASURE - number_traces_max_t);
        return false;
    }

    /* max_t: the t statistic value
     * max_tau: a t value normalized by sqrt(number of measurements).
     *          this way we can compare max_tau taken with different
     *          number of measurements. This is sort of "distance
     *          between distributions", independent of number of
     *          measurements.
     * (5/tau)^2: how many measurements we would need to barely
     *            detect the leak, if present. "barely detect the
     *            leak" = have a t value greater than 5.
     */
    printf("max t: %+7.2f, max tau: %.2e, (5/tau)^2: %.2e.\n", max_t, max_tau,
           (double) (5 * 5) / (double) (max_tau * max_tau));

    /* Definitely not constant time */
    if (max_t > t_threshold_bananas)
        return false;

    /* Probably not constant time. */
    if (max_t > t_threshold_moderate)
        return false;

    /* For the moment, maybe constant time. */
    return true;
}

static bool doit(int mode)
{
    int64_t *before_ticks = calloc(N_MEASURES + 1, sizeof(int64_t));
    int64_t *after_ticks = calloc(N_MEASURES + 1, sizeof(int64_t));
    int64_t *exec_times = calloc(N_MEASURES, sizeof(int64_t));
    uint8_t *classes = calloc(N_MEASURES, sizeof(uint8_t));
    uint8_t *input_data = calloc(N_MEASURES * CHUNK_SIZE, sizeof(uint8_t));
    int64_t *percentiles = calloc(N_MEASURES, sizeof(int64_t));

    if (!before_ticks || !after_ticks || !exec_times || !classes ||
        !input_data || !percentiles) {
        die();
    }

    prepare_inputs(input_data, classes);
    bool ret = measure(before_ticks, after_ticks, input_data, mode);
    bool first_time = percentiles[NUMBER_PERCENTILES - 1] == 0;
    differentiate(exec_times, before_ticks, after_ticks);
    if (first_time) {
        // throw away the first batch of measurements.
        // this helps warming things up.
        prepare_percentiles(percentiles, exec_times);
    } else {
        update_statistics(exec_times, classes, percentiles);
        ret &= report();
    }

    free(before_ticks);
    free(after_ticks);
    free(exec_times);
    free(classes);
    free(input_data);
    free(percentiles);

    return ret;
}

static void init_once(void)
{
    init_dut();
    for (int i = 0; i < DUDECT_TESTS; i++)
        t_init(t[i]);
}

static bool test_const(char *text, int mode)
{
    bool result = false;
    for (int i = 0; i < DUDECT_TESTS; i++)
        t[i] = malloc(sizeof(t_context_t));

    for (int cnt = 0; cnt < TEST_TRIES; ++cnt) {
        printf("Testing %s...(%d/%d)\n\n", text, cnt, TEST_TRIES);
        init_once();
        for (int i = 0; i < ENOUGH_MEASURE / (N_MEASURES - DROP_SIZE * 2) + 1;
             ++i)
            result = doit(mode);
        printf("\033[A\033[2K\033[A\033[2K");
        if (result)
            break;
    }
    for (int j = 0; j < DUDECT_TESTS; j++)
        free(t[j]);
    return result;
}

#define DUT_FUNC_IMPL(op)                \
    bool is_##op##_const(void)           \
    {                                    \
        return test_const(#op, DUT(op)); \
    }

#define _(x) DUT_FUNC_IMPL(x)
DUT_FUNCS
#undef _
