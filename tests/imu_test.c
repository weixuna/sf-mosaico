/*
 * IMU shell 自测
 *
 *   imu_test id              读 WHO_AM_I，期望 0x6A
 *   imu_test once            读一帧 accel / gyro / temp
 *   imu_test stream [ms]     连续打印，默认 100 ms
 *   imu_test gesture         打印 shake / tilt / flip 事件
 *   imu_test stop            停止 stream / gesture
 *   imu_test reg <addr>      读一个寄存器
 *   imu_test wreg <addr> <val>  写一个寄存器（调试用）
 */

#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lsm6ds3tr_c.h"
#include "imu_service.h"

#define IMU_TEST_WAIT_MS        500
#define IMU_TEST_DEFAULT_MS     100
#define IMU_TEST_STACK_SIZE     2048
#define IMU_TEST_PRIORITY       25

static volatile rt_bool_t s_stream_on = RT_FALSE;
static volatile uint32_t s_stream_period_ms = IMU_TEST_DEFAULT_MS;
static rt_thread_t s_stream_thread = RT_NULL;

/* 用整数打印，避免 rt_kprintf 不支持 %f */
static void print_fixed(float value)
{
    int32_t milli = (int32_t)(value * 1000.0f +
                              ((value < 0.0f) ? -0.5f : 0.5f));

    if (milli < 0)
    {
        rt_kprintf("-%d.%03d", (int)(-milli / 1000), (int)(-milli % 1000));
    }
    else
    {
        rt_kprintf("+%d.%03d", (int)(milli / 1000), (int)(milli % 1000));
    }
}

static void print_vec(const char *name, const lsm6ds3tr_c_vec3_t *v)
{
    rt_kprintf("%s=", name);
    print_fixed(v->x);
    rt_kprintf(" ");
    print_fixed(v->y);
    rt_kprintf(" ");
    print_fixed(v->z);
}

static void print_snapshot(const imu_snapshot_t *snap)
{
    float mag = sqrtf(snap->accel_g.x * snap->accel_g.x +
                      snap->accel_g.y * snap->accel_g.y +
                      snap->accel_g.z * snap->accel_g.z);

    rt_kprintf("[IMU] ");
    print_vec("accel[g]", &snap->accel_g);
    rt_kprintf(" |a|=");
    print_fixed(mag);
    rt_kprintf("\n[IMU] ");
    print_vec("gyro[dps]", &snap->gyro_dps);
    rt_kprintf(" temp=");
    print_fixed(snap->temp_c);
    rt_kprintf("C\n");
}

static int wait_snapshot(imu_snapshot_t *snap, uint32_t timeout_ms)
{
    uint32_t start = rt_tick_get_millisecond();

    do
    {
        if ((imu_service_get(snap) == 0) && snap->valid)
        {
            return 0;
        }
        rt_thread_mdelay(10);
    } while ((rt_tick_get_millisecond() - start) < timeout_ms);

    return -1;
}

static void stream_thread_entry(void *arg)
{
    (void)arg;

    for (;;)
    {
        if (s_stream_on)
        {
            imu_snapshot_t snap;

            if ((imu_service_get(&snap) == 0) && snap.valid)
            {
                print_snapshot(&snap);
            }
            rt_thread_mdelay(s_stream_period_ms);
        }
        else
        {
            rt_thread_mdelay(50);
        }
    }
}

static void stream_start(uint32_t period_ms)
{
    imu_service_subscribe(RT_NULL, RT_NULL);
    s_stream_period_ms = period_ms ? period_ms : IMU_TEST_DEFAULT_MS;

    if (s_stream_thread == RT_NULL)
    {
        s_stream_thread = rt_thread_create("imu_test", stream_thread_entry,
                                           RT_NULL, IMU_TEST_STACK_SIZE,
                                           IMU_TEST_PRIORITY, 10);
        if (s_stream_thread == RT_NULL)
        {
            rt_kprintf("[IMU] stream thread create failed\n");
            return;
        }
        rt_thread_startup(s_stream_thread);
    }

    s_stream_on = RT_TRUE;
    rt_kprintf("[IMU] stream %d ms, 'imu_test stop' to quit\n",
               (int)s_stream_period_ms);
}

static const char *gesture_name(imu_event_type_t type)
{
    switch (type)
    {
    case IMU_EVT_SHAKE:
        return "shake";
    case IMU_EVT_TILT:
        return "tilt";
    case IMU_EVT_FLIP:
        return "flip";
    default:
        return "unknown";
    }
}

static void gesture_event_cb(const imu_event_t *evt, void *ctx)
{
    lsm6ds3tr_c_vec3_t accel = {evt->ax, evt->ay, evt->az};
    float mag = sqrtf(accel.x * accel.x + accel.y * accel.y +
                      accel.z * accel.z);

    (void)ctx;

    rt_kprintf("[IMU] gesture=%s ", gesture_name(evt->type));
    print_vec("accel[g]", &accel);
    rt_kprintf(" |a|=");
    print_fixed(mag);
    rt_kprintf("\n");
}

static void gesture_start(void)
{
    s_stream_on = RT_FALSE;

    if (imu_service_subscribe(gesture_event_cb, RT_NULL) != 0)
    {
        rt_kprintf("[IMU] gesture subscribe failed\n");
        return;
    }

    rt_kprintf("[IMU] gesture: shake / tilt / flip, "
               "'imu_test stop' to quit\n");
}

static void imu_test_id(void)
{
    uint8_t id = 0;

    if (imu_service_read_reg(LSM6DS3TR_C_REG_WHO_AM_I, &id) != 0)
    {
        rt_kprintf("[IMU] read WHO_AM_I failed\n");
        return;
    }

    rt_kprintf("[IMU] who=0x%02X addr=0x%02X\n", id, LSM6DS3TR_C_I2C_ADDR);
}

static void imu_test_once(void)
{
    imu_snapshot_t snap;

    if (wait_snapshot(&snap, IMU_TEST_WAIT_MS) != 0)
    {
        rt_kprintf("[IMU] no sample, check sensor power / I2C\n");
        return;
    }

    print_snapshot(&snap);
}

static void imu_test_reg(int argc, char **argv)
{
    unsigned long reg;
    uint8_t val = 0;

    if (argc < 3)
    {
        rt_kprintf("usage: imu_test reg <addr>\n");
        return;
    }

    reg = strtoul(argv[2], NULL, 16);
    if ((reg > 0xFF) || (imu_service_read_reg((uint8_t)reg, &val) != 0))
    {
        rt_kprintf("[IMU] read reg failed\n");
        return;
    }

    rt_kprintf("[IMU] reg[0x%02X]=0x%02X\n", (unsigned)reg, val);
}

static void imu_test_wreg(int argc, char **argv)
{
    unsigned long reg;
    unsigned long val;

    if (argc < 4)
    {
        rt_kprintf("usage: imu_test wreg <addr> <val>\n");
        return;
    }

    reg = strtoul(argv[2], NULL, 16);
    val = strtoul(argv[3], NULL, 16);
    if ((reg > 0xFF) || (val > 0xFF) ||
            (imu_service_write_reg((uint8_t)reg, (uint8_t)val) != 0))
    {
        rt_kprintf("[IMU] write reg failed\n");
        return;
    }

    rt_kprintf("[IMU] reg[0x%02X]=0x%02X written\n",
               (unsigned)reg, (unsigned)val);
}

static void imu_test_usage(void)
{
    rt_kprintf("usage: imu_test id|once|stream [ms]|gesture|stop|"
               "reg <addr>|wreg <addr> <val>\n");
}

static void imu_test_shell(int argc, char **argv)
{
    if (argc < 2)
    {
        imu_test_usage();
        return;
    }

    if (imu_service_init() != 0)
    {
        rt_kprintf("[IMU] service init failed\n");
        return;
    }

    if (strcmp(argv[1], "id") == 0)
    {
        imu_test_id();
    }
    else if (strcmp(argv[1], "once") == 0)
    {
        imu_test_once();
    }
    else if (strcmp(argv[1], "stream") == 0)
    {
        uint32_t period_ms = (argc >= 3) ?
                             (uint32_t)strtoul(argv[2], NULL, 10) : 0;
        stream_start(period_ms);
    }
    else if (strcmp(argv[1], "gesture") == 0)
    {
        gesture_start();
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        s_stream_on = RT_FALSE;
        imu_service_subscribe(RT_NULL, RT_NULL);
        rt_kprintf("[IMU] stopped\n");
    }
    else if (strcmp(argv[1], "reg") == 0)
    {
        imu_test_reg(argc, argv);
    }
    else if (strcmp(argv[1], "wreg") == 0)
    {
        imu_test_wreg(argc, argv);
    }
    else
    {
        imu_test_usage();
    }
}
MSH_CMD_EXPORT_ALIAS(imu_test_shell, imu_test, IMU self-test);
