/*
 * 双 BMM150 磁力计服务
 *
 * 10 Hz 采样，4 点滑动平均，上电后自动采集 16 帧基线。
 * 基线就绪后计算两路增量并运行单机边缘分类器。
 */

#include <rtthread.h>
#include <string.h>

#include "mag_service.h"

#define DBG_TAG "sensor.mag"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define MAG_SAMPLE_PERIOD_MS    100
#define MAG_BASELINE_SAMPLES    16
#define MAG_BASELINE_MAX_SPAN   150     /* uT，超过认为基线不稳定 */
#define MAG_FILTER_DEPTH        4
#define MAG_READ_FAIL_LIMIT     3
#define MAG_RETRY_DELAY_MS      1000
#define MAG_STACK_SIZE          3072
#define MAG_THREAD_PRIORITY     20

typedef struct
{
    int16_t data[MAG_FILTER_DEPTH][3];
    uint8_t index;
    uint8_t count;
} mag_filter_t;

static bmm150_t s_right;
static bmm150_t s_left;
static mag_filter_t s_filter_right;
static mag_filter_t s_filter_left;

static mag_pair_t s_pair;
static mag_features_t s_features;
static uint8_t s_features_valid;
static uint8_t s_edges;

static int32_t s_base_right[3];
static int32_t s_base_left[3];
static int32_t s_sum_right[3];
static int32_t s_sum_left[3];
static int16_t s_min_right[3];
static int16_t s_max_right[3];
static int16_t s_min_left[3];
static int16_t s_max_left[3];
static uint8_t s_base_count;
static uint8_t s_baseline_valid;

static mag_classifier_t s_classifier;
static volatile uint8_t s_calibrate_request;

static struct rt_mutex s_lock;
static rt_bool_t s_lock_ready = RT_FALSE;
static rt_bool_t s_inited = RT_FALSE;

static mag_event_cb_t s_event_cb;
static void *s_event_ctx;

static void mag_filter_reset(mag_filter_t *filter)
{
    filter->index = 0;
    filter->count = 0;
}

static void mag_filter_update(mag_filter_t *filter,
                              const bmm150_sample_t *in,
                              bmm150_sample_t *out)
{
    int32_t sum[3] = {0, 0, 0};
    uint8_t i;

    filter->data[filter->index][0] = in->field.x;
    filter->data[filter->index][1] = in->field.y;
    filter->data[filter->index][2] = in->field.z;
    filter->index = (uint8_t)((filter->index + 1) % MAG_FILTER_DEPTH);

    if (filter->count < MAG_FILTER_DEPTH)
    {
        filter->count++;
    }

    for (i = 0; i < filter->count; i++)
    {
        sum[0] += filter->data[i][0];
        sum[1] += filter->data[i][1];
        sum[2] += filter->data[i][2];
    }

    out->field.x = (int16_t)(sum[0] / filter->count);
    out->field.y = (int16_t)(sum[1] / filter->count);
    out->field.z = (int16_t)(sum[2] / filter->count);
    out->rhall = in->rhall;
    out->valid = in->valid;
}

/* 清空基线采集；同时让特征接口立即失效 */
static void mag_baseline_reset(void)
{
    memset(s_sum_right, 0, sizeof(s_sum_right));
    memset(s_sum_left, 0, sizeof(s_sum_left));
    s_base_count = 0;
    s_baseline_valid = 0;

    if (s_lock_ready &&
            (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) == RT_EOK))
    {
        s_features_valid = 0;
        s_edges = 0;
        rt_mutex_release(&s_lock);
    }
}

static void mag_baseline_update(const mag_pair_t *pair)
{
    int16_t r[3] = {pair->right.field.x, pair->right.field.y,
                    pair->right.field.z};
    int16_t l[3] = {pair->left.field.x, pair->left.field.y,
                    pair->left.field.z};
    uint8_t i;

    if (!pair->valid)
    {
        return;
    }

    for (i = 0; i < 3; i++)
    {
        s_sum_right[i] += r[i];
        s_sum_left[i] += l[i];

        if (s_base_count == 0)
        {
            s_min_right[i] = r[i];
            s_max_right[i] = r[i];
            s_min_left[i] = l[i];
            s_max_left[i] = l[i];
        }
        else
        {
            if (r[i] < s_min_right[i]) s_min_right[i] = r[i];
            if (r[i] > s_max_right[i]) s_max_right[i] = r[i];
            if (l[i] < s_min_left[i]) s_min_left[i] = l[i];
            if (l[i] > s_max_left[i]) s_max_left[i] = l[i];
        }
    }

    s_base_count++;

    if (s_base_count < MAG_BASELINE_SAMPLES)
    {
        return;
    }

    for (i = 0; i < 3; i++)
    {
        if (((s_max_right[i] - s_min_right[i]) > MAG_BASELINE_MAX_SPAN) ||
                ((s_max_left[i] - s_min_left[i]) > MAG_BASELINE_MAX_SPAN))
        {
            LOG_W("baseline unstable, retrying");
            mag_baseline_reset();
            return;
        }
    }

    for (i = 0; i < 3; i++)
    {
        s_base_right[i] = s_sum_right[i] / s_base_count;
        s_base_left[i] = s_sum_left[i] / s_base_count;
    }

    s_baseline_valid = 1;
    LOG_I("MAG baseline ready");
}

static void mag_compute_features(const mag_pair_t *pair, mag_features_t *f)
{
    int16_t r[3] = {pair->right.field.x, pair->right.field.y,
                    pair->right.field.z};
    int16_t l[3] = {pair->left.field.x, pair->left.field.y,
                    pair->left.field.z};
    int32_t v;
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        f->right_delta[i] = (int16_t)(r[i] - s_base_right[i]);
        f->left_delta[i] = (int16_t)(l[i] - s_base_left[i]);
        f->diff[i] = (int16_t)(f->right_delta[i] - f->left_delta[i]);
    }

    f->right_mag2 = 0;
    f->left_mag2 = 0;
    f->diff_mag2 = 0;

    for (i = 0; i < 3; i++)
    {
        v = (int32_t)r[i] * r[i];
        f->right_mag2 += (uint32_t)v;
        v = (int32_t)l[i] * l[i];
        f->left_mag2 += (uint32_t)v;
        v = (int32_t)f->diff[i] * f->diff[i];
        f->diff_mag2 += (uint32_t)v;
    }
}

/* 更新共享快照；事件回调在锁外执行 */
static void mag_publish(const mag_pair_t *pair,
                        const mag_features_t *features,
                        uint8_t features_valid,
                        uint8_t edges,
                        const mag_event_t *evt)
{
    mag_event_cb_t cb = RT_NULL;
    void *ctx = RT_NULL;

    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) == RT_EOK)
    {
        s_pair = *pair;

        if (features_valid && (features != RT_NULL))
        {
            s_features = *features;
        }

        s_features_valid = features_valid;
        s_edges = edges;
        cb = s_event_cb;
        ctx = s_event_ctx;

        rt_mutex_release(&s_lock);
    }

    if ((evt != RT_NULL) && (cb != RT_NULL))
    {
        cb(evt, ctx);
    }
}

static void mag_process(const bmm150_sample_t *right,
                        const bmm150_sample_t *left,
                        uint32_t now_ms)
{
    mag_pair_t pair;
    mag_features_t features;
    mag_event_t evt;
    int has_event;

    mag_filter_update(&s_filter_right, right, &pair.right);
    mag_filter_update(&s_filter_left, left, &pair.left);
    pair.tick_ms = now_ms;
    pair.valid = pair.right.valid && pair.left.valid;

    if (!s_baseline_valid)
    {
        mag_publish(&pair, RT_NULL, 0, 0, RT_NULL);
        mag_baseline_update(&pair);
        return;
    }

    mag_compute_features(&pair, &features);
    has_event = mag_classifier_update(&s_classifier, &features, now_ms, &evt);

    mag_publish(&pair, &features, 1,
                mag_classifier_edges(&s_classifier),
                has_event ? &evt : RT_NULL);
}

static int mag_ensure_started(void)
{
    if (!s_right.initialized)
    {
        if (bmm150_init(&s_right, RT_NULL,
                        BMM150_I2C_ADDR_RIGHT) != 0)
        {
            return -1;
        }
        mag_filter_reset(&s_filter_right);
    }

    if (!s_left.initialized)
    {
        if (bmm150_init(&s_left, RT_NULL,
                        BMM150_I2C_ADDR_LEFT) != 0)
        {
            return -1;
        }
        mag_filter_reset(&s_filter_left);
    }

    return 0;
}

static void mag_thread_entry(void *arg)
{
    uint8_t fail_count = 0;
    rt_bool_t running = RT_FALSE;

    (void)arg;

    for (;;)
    {
        if (s_calibrate_request)
        {
            s_calibrate_request = 0;
            mag_filter_reset(&s_filter_right);
            mag_filter_reset(&s_filter_left);
            mag_classifier_init(&s_classifier, RT_NULL);
            mag_baseline_reset();
            LOG_I("MAG baseline recapture");
        }

        if (mag_ensure_started() != 0)
        {
            if (running)
            {
                LOG_W("MAG lost, retrying");
                running = RT_FALSE;
            }
            rt_thread_mdelay(MAG_RETRY_DELAY_MS);
            continue;
        }

        if (!running)
        {
            LOG_I("MAG ready");
            running = RT_TRUE;
        }

        {
            bmm150_sample_t right;
            bmm150_sample_t left;

            if ((bmm150_read(&s_right, &right) == 0) &&
                    (bmm150_read(&s_left, &left) == 0))
            {
                fail_count = 0;

                if (right.valid && left.valid)
                {
                    mag_process(&right, &left,
                                rt_tick_get_millisecond());
                }
            }
            else if (++fail_count >= MAG_READ_FAIL_LIMIT)
            {
                LOG_W("MAG read failed, reinit");
                bmm150_deinit(&s_right);
                bmm150_deinit(&s_left);
                fail_count = 0;
                running = RT_FALSE;
            }
        }

        rt_thread_mdelay(MAG_SAMPLE_PERIOD_MS);
    }
}

int mag_service_init(void)
{
    rt_thread_t tid;

    if (s_inited)
    {
        return 0;
    }

    if (!s_lock_ready)
    {
        if (rt_mutex_init(&s_lock, "mag", RT_IPC_FLAG_PRIO) != RT_EOK)
        {
            LOG_E("mutex init failed");
            return -1;
        }
        s_lock_ready = RT_TRUE;
    }

    mag_classifier_init(&s_classifier, RT_NULL);
    mag_filter_reset(&s_filter_right);
    mag_filter_reset(&s_filter_left);
    mag_baseline_reset();

    /* 先同步探测一次，失败由采样线程重试 */
    (void)mag_ensure_started();

    tid = rt_thread_create("mag", mag_thread_entry, RT_NULL,
                           MAG_STACK_SIZE, MAG_THREAD_PRIORITY, 10);
    if (tid == RT_NULL)
    {
        LOG_E("thread create failed");
        return -1;
    }

    rt_thread_startup(tid);
    s_inited = RT_TRUE;

    return 0;
}

int mag_service_get(mag_pair_t *out)
{
    if ((out == RT_NULL) || !s_lock_ready)
    {
        return -1;
    }

    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) != RT_EOK)
    {
        return -1;
    }

    *out = s_pair;

    rt_mutex_release(&s_lock);

    return 0;
}

int mag_service_get_features(mag_features_t *out)
{
    if ((out == RT_NULL) || !s_lock_ready)
    {
        return -1;
    }

    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) != RT_EOK)
    {
        return -1;
    }

    if (!s_features_valid)
    {
        rt_mutex_release(&s_lock);
        return -1;
    }

    *out = s_features;

    rt_mutex_release(&s_lock);

    return 0;
}

uint8_t mag_service_get_edges(void)
{
    uint8_t edges = 0;

    if (!s_lock_ready)
    {
        return 0;
    }

    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) == RT_EOK)
    {
        edges = s_edges;
        rt_mutex_release(&s_lock);
    }

    return edges;
}

int mag_service_calibrate(void)
{
    if (!s_lock_ready)
    {
        return -1;
    }

    s_calibrate_request = 1;

    return 0;
}

int mag_service_subscribe(mag_event_cb_t cb, void *ctx)
{
    if (!s_lock_ready)
    {
        return -1;
    }

    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) != RT_EOK)
    {
        return -1;
    }

    s_event_cb = cb;
    s_event_ctx = ctx;

    rt_mutex_release(&s_lock);

    return 0;
}

INIT_APP_EXPORT(mag_service_init);
