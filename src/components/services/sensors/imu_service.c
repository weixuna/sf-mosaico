/*
 * IMU 采样服务
 *
 * 40 ms 采样、600 ms 手势冷却，行为与 Mosaico imu_gesture 对齐。
 */

#include <rtthread.h>
#include "imu_service.h"
#include "imu_gesture.h"

#define DBG_TAG "sensor.imu"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define IMU_SAMPLE_PERIOD_MS    40
#define IMU_STARTUP_DELAY_MS    80
#define IMU_RETRY_DELAY_MS      1000
#define IMU_READ_FAIL_LIMIT     3
#define IMU_STACK_SIZE          3072
#define IMU_THREAD_PRIORITY     20

static lsm6ds3tr_c_t s_dev;
static imu_snapshot_t s_snapshot;
static imu_gesture_t s_gesture;

static struct rt_mutex s_lock;
static rt_bool_t s_lock_ready = RT_FALSE;
static rt_bool_t s_inited = RT_FALSE;

static imu_event_cb_t s_event_cb;
static void *s_event_ctx;

/* 初始化并启动传感器；失败时由采样线程稍后重试 */
static int imu_ensure_started(void)
{
    if (!s_dev.initialized)
    {
        if (lsm6ds3tr_c_init(&s_dev, RT_NULL, LSM6DS3TR_C_I2C_ADDR) != 0)
        {
            return -1;
        }
    }

    if (!s_dev.started)
    {
        if (lsm6ds3tr_c_start(&s_dev, RT_NULL) != 0)
        {
            return -1;
        }

        /* Mosaico 启动后等待 80 ms，跳过无效的首帧 */
        rt_thread_mdelay(IMU_STARTUP_DELAY_MS);
    }

    return 0;
}

static void imu_update_snapshot(const lsm6ds3tr_c_sample_t *sample,
                                uint32_t now_ms)
{
    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) != RT_EOK)
    {
        return;
    }

    s_snapshot.accel_g = sample->accel_g;
    s_snapshot.gyro_dps = sample->gyro_dps;
    s_snapshot.temp_c = sample->temp_c;
    s_snapshot.tick_ms = now_ms;
    s_snapshot.valid = 1;

    rt_mutex_release(&s_lock);
}

static imu_event_type_t gesture_to_event(imu_gesture_type_t gesture)
{
    switch (gesture)
    {
    case IMU_GESTURE_SHAKE:
        return IMU_EVT_SHAKE;
    case IMU_GESTURE_TILT:
        return IMU_EVT_TILT;
    case IMU_GESTURE_FLIP:
        return IMU_EVT_FLIP;
    default:
        return IMU_EVT_SHAKE;
    }
}

static void imu_process_gesture(const lsm6ds3tr_c_sample_t *sample,
                                uint32_t now_ms)
{
    imu_gesture_type_t gesture;
    imu_event_cb_t cb;
    void *ctx;
    imu_event_t evt;

    gesture = imu_gesture_update(&s_gesture,
                                 sample->accel_g.x,
                                 sample->accel_g.y,
                                 sample->accel_g.z,
                                 now_ms);
    if (gesture == IMU_GESTURE_NONE)
    {
        return;
    }

    evt.type = gesture_to_event(gesture);
    evt.tick_ms = now_ms;
    evt.ax = sample->accel_g.x;
    evt.ay = sample->accel_g.y;
    evt.az = sample->accel_g.z;

    /* 回调在锁外执行，避免订阅者间接读取快照时死锁 */
    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) != RT_EOK)
    {
        return;
    }
    cb = s_event_cb;
    ctx = s_event_ctx;
    rt_mutex_release(&s_lock);

    if (cb != RT_NULL)
    {
        cb(&evt, ctx);
    }
}

static void imu_thread_entry(void *arg)
{
    lsm6ds3tr_c_sample_t sample;
    uint8_t fail_count = 0;
    uint32_t period_ms;
    rt_bool_t running = RT_FALSE;

    (void)arg;

    for (;;)
    {
        if (imu_ensure_started() != 0)
        {
            if (running)
            {
                LOG_W("IMU lost, retrying");
                running = RT_FALSE;
            }
            rt_thread_mdelay(IMU_RETRY_DELAY_MS);
            continue;
        }

        if (!running)
        {
            LOG_I("IMU ready");
            running = RT_TRUE;
        }

        if (lsm6ds3tr_c_read(&s_dev, &sample) == 0)
        {
            uint32_t now_ms = rt_tick_get_millisecond();

            fail_count = 0;
            imu_update_snapshot(&sample, now_ms);
            imu_process_gesture(&sample, now_ms);
        }
        else if (++fail_count >= IMU_READ_FAIL_LIMIT)
        {
            /* 连续失败时重新初始化，和文档的恢复策略一致 */
            LOG_W("read failed %d times, restarting", fail_count);
            lsm6ds3tr_c_stop(&s_dev);
            fail_count = 0;
            running = RT_FALSE;
        }

        period_ms = s_gesture.config.sample_period_ms;
        rt_thread_mdelay(period_ms ? period_ms : IMU_SAMPLE_PERIOD_MS);
    }
}

int imu_service_init(void)
{
    rt_thread_t tid;

    if (s_inited)
    {
        return 0;
    }

    if (!s_lock_ready)
    {
        if (rt_mutex_init(&s_lock, "imu", RT_IPC_FLAG_PRIO) != RT_EOK)
        {
            LOG_E("mutex init failed");
            return -1;
        }
        s_lock_ready = RT_TRUE;
    }

    imu_gesture_init(&s_gesture, RT_NULL);

    /* 先同步尝试一次，shell 在启动后立刻读寄存器也能成功；失败由线程重试 */
    (void)imu_ensure_started();

    tid = rt_thread_create("imu", imu_thread_entry, RT_NULL,
                           IMU_STACK_SIZE, IMU_THREAD_PRIORITY, 10);
    if (tid == RT_NULL)
    {
        LOG_E("thread create failed");
        return -1;
    }

    rt_thread_startup(tid);
    s_inited = RT_TRUE;

    return 0;
}

int imu_service_get(imu_snapshot_t *out)
{
    if ((out == RT_NULL) || !s_lock_ready)
    {
        return -1;
    }

    if (rt_mutex_take(&s_lock, rt_tick_from_millisecond(100)) != RT_EOK)
    {
        return -1;
    }

    *out = s_snapshot;

    rt_mutex_release(&s_lock);

    return 0;
}

int imu_service_subscribe(imu_event_cb_t cb, void *ctx)
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

int imu_service_read_reg(uint8_t reg, uint8_t *val)
{
    if (!s_dev.initialized)
    {
        return -1;
    }

    return lsm6ds3tr_c_read_reg(&s_dev, reg, val, 1);
}

int imu_service_write_reg(uint8_t reg, uint8_t val)
{
    if (!s_dev.initialized)
    {
        return -1;
    }

    return lsm6ds3tr_c_write_reg(&s_dev, reg, &val, 1);
}

INIT_APP_EXPORT(imu_service_init);
