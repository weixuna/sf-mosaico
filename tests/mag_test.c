/*
 * BMM150 磁力计 shell 自测
 *
 *   mag_test              读一次双路快照 + 特征 + 当前边缘
 *   mag_test <n>          连续打印 n 次，100 ms 间隔
 *   mag_test baseline     重新采集基线（保持无磁吸）
 *   mag_test events       打印 APPROACH / CONTACT / RELEASE
 *   mag_test stop         停止连续输出 / 事件
 */

#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

#include "mag_service.h"

#define MAG_TEST_PERIOD_MS      100
#define MAG_TEST_WAIT_MS        1000
#define MAG_TEST_STACK_SIZE     2048
#define MAG_TEST_PRIORITY       25

static volatile rt_bool_t s_stream_on = RT_FALSE;
static volatile uint32_t s_stream_left;
static rt_thread_t s_stream_thread = RT_NULL;

static void print_pair(const mag_pair_t *pair)
{
    rt_kprintf("[MAG] R 0x%02X: x=%d y=%d z=%d rhall=%u valid=%u\n",
               BMM150_I2C_ADDR_RIGHT,
               pair->right.field.x, pair->right.field.y,
               pair->right.field.z, pair->right.rhall,
               pair->right.valid);
    rt_kprintf("[MAG] L 0x%02X: x=%d y=%d z=%d rhall=%u valid=%u\n",
               BMM150_I2C_ADDR_LEFT,
               pair->left.field.x, pair->left.field.y,
               pair->left.field.z, pair->left.rhall,
               pair->left.valid);
    rt_kprintf("[MAG] dR-L: x=%d y=%d z=%d\n",
               pair->right.field.x - pair->left.field.x,
               pair->right.field.y - pair->left.field.y,
               pair->right.field.z - pair->left.field.z);
}

static void print_features(const mag_features_t *features)
{
    rt_kprintf("[MAG] dR: x=%d y=%d z=%d\n",
               features->right_delta[0], features->right_delta[1],
               features->right_delta[2]);
    rt_kprintf("[MAG] dL: x=%d y=%d z=%d\n",
               features->left_delta[0], features->left_delta[1],
               features->left_delta[2]);
    rt_kprintf("[MAG] diff: x=%d y=%d z=%d\n",
               features->diff[0], features->diff[1], features->diff[2]);
    rt_kprintf("[MAG] |B|^2: R=%u L=%u diff=%u\n",
               (unsigned)features->right_mag2,
               (unsigned)features->left_mag2,
               (unsigned)features->diff_mag2);
}

static void print_state(void)
{
    rt_kprintf("[MAG] edge=0x%02X\n", mag_service_get_edges());
}

static int wait_pair(mag_pair_t *pair, uint32_t timeout_ms)
{
    uint32_t start = rt_tick_get_millisecond();

    do
    {
        if ((mag_service_get(pair) == 0) && pair->valid)
        {
            return 0;
        }
        rt_thread_mdelay(10);
    } while ((rt_tick_get_millisecond() - start) < timeout_ms);

    return -1;
}

static void mag_test_once(void)
{
    mag_pair_t pair;
    mag_features_t features;

    if (wait_pair(&pair, MAG_TEST_WAIT_MS) != 0)
    {
        rt_kprintf("[MAG] no valid sample, check sensor power / I2C\n");
        return;
    }

    print_pair(&pair);

    if (mag_service_get_features(&features) == 0)
    {
        print_features(&features);
    }
    else
    {
        rt_kprintf("[MAG] baseline pending, retry or use 'mag_test 20'\n");
    }

    print_state();
}

static void stream_thread_entry(void *arg)
{
    (void)arg;

    for (;;)
    {
        if (s_stream_on)
        {
            mag_pair_t pair;
            mag_features_t features;

            if ((mag_service_get(&pair) == 0) && pair.valid)
            {
                print_pair(&pair);

                if (mag_service_get_features(&features) == 0)
                {
                    print_features(&features);
                }
                else
                {
                    rt_kprintf("[MAG] baseline pending\n");
                }

                print_state();
            }
            else
            {
                rt_kprintf("[MAG] no valid sample\n");
            }

            if (s_stream_left > 0)
            {
                s_stream_left--;

                if (s_stream_left == 0)
                {
                    s_stream_on = RT_FALSE;
                    rt_kprintf("[MAG] stream done\n");
                }
            }

            rt_thread_mdelay(MAG_TEST_PERIOD_MS);
        }
        else
        {
            rt_thread_mdelay(50);
        }
    }
}

static void stream_start(uint32_t count)
{
    mag_service_subscribe(RT_NULL, RT_NULL);
    s_stream_left = (count == 0) ? 1 : count;

    if (s_stream_thread == RT_NULL)
    {
        s_stream_thread = rt_thread_create("mag_test", stream_thread_entry,
                                           RT_NULL, MAG_TEST_STACK_SIZE,
                                           MAG_TEST_PRIORITY, 10);
        if (s_stream_thread == RT_NULL)
        {
            rt_kprintf("[MAG] stream thread create failed\n");
            return;
        }
        rt_thread_startup(s_stream_thread);
    }

    s_stream_on = RT_TRUE;
    rt_kprintf("[MAG] stream %u samples, 'mag_test stop' to quit\n",
               (unsigned)s_stream_left);
}

static const char *event_name(mag_event_type_t type)
{
    switch (type)
    {
    case MAG_EVT_APPROACH:
        return "approach";
    case MAG_EVT_CONTACT:
        return "contact";
    case MAG_EVT_RELEASE:
        return "release";
    default:
        return "unknown";
    }
}

static void event_cb(const mag_event_t *evt, void *ctx)
{
    (void)ctx;

    rt_kprintf("[MAG] event=%s edge=0x%02X confidence=%u tick=%u\n",
               event_name(evt->type), evt->edge, evt->confidence,
               (unsigned)evt->tick_ms);
}

static void events_start(void)
{
    s_stream_on = RT_FALSE;

    if (mag_service_subscribe(event_cb, RT_NULL) != 0)
    {
        rt_kprintf("[MAG] subscribe failed\n");
        return;
    }

    rt_kprintf("[MAG] events on, 'mag_test stop' to quit\n");
}

static void mag_test_usage(void)
{
    rt_kprintf("usage: mag_test [count] | baseline | events | stop\n");
}

static void mag_test_shell(int argc, char **argv)
{
    if (mag_service_init() != 0)
    {
        rt_kprintf("[MAG] service init failed\n");
        return;
    }

    if (argc < 2)
    {
        mag_test_once();
    }
    else if (strcmp(argv[1], "baseline") == 0)
    {
        if (mag_service_calibrate() != 0)
        {
            rt_kprintf("[MAG] calibrate failed\n");
        }
        else
        {
            rt_kprintf("[MAG] recapturing baseline, keep magnets away\n");
        }
    }
    else if (strcmp(argv[1], "events") == 0)
    {
        events_start();
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        s_stream_on = RT_FALSE;
        mag_service_subscribe(RT_NULL, RT_NULL);
        rt_kprintf("[MAG] stopped\n");
    }
    else
    {
        unsigned long count = strtoul(argv[1], NULL, 10);

        if (count == 0)
        {
            mag_test_usage();
        }
        else
        {
            stream_start((uint32_t)count);
        }
    }
}
MSH_CMD_EXPORT_ALIAS(mag_test_shell, mag_test, BMM150 self-test);
