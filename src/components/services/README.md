# services

跨模块服务层。这里放“数据怎么流转、状态怎么维护”，不放 LVGL，也不放寄存器操作。

建议的服务：

| 服务 | 职责 |
|---|---|
| `sensors/imu_service` | LSM6DS3TR-C 周期采样、快照、shake/tilt/flip 事件 |
| `sensors/mag_service` | 双 BMM150 采样、基线、滤波、磁吸事件 |
| `sensors/mag_classifier` | 单机磁吸边缘分类和去抖 |
| `sensor_service` | 统一 IMU/MAG/充电快照（后续） |
| `audio_service` | 播放/录音队列、音量、PA/MIC 电源 |
| `power_service` | 休眠唤醒、电源轨、充电状态 |
| `settings_service` | 亮度、语言、表盘等配置持久化 |

约束：

- 对外只暴露非阻塞 API，例如：
  ```c
  int sensor_service_init(void);
  int sensor_service_get(sensor_snapshot_t *out);
  int audio_service_play(audio_id_t id);
  ```
- 不在服务里调用 `lv_*`。
- 不直接暴露 `rt_device_t` 给 app。
- 线程间用 `rt_mq` / `rt_event` / 互斥锁，不用裸全局变量。
