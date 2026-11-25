# tests

硬件自测代码。当前包含 IMU 自测和磁力计自测。

## mag_test

```text
mag_test                    读一次双路快照 + 特征 + 当前边缘
mag_test <n>                连续打印 n 次，100 ms 间隔
mag_test baseline           重新采集基线（保持无磁吸）
mag_test events             打印 APPROACH / CONTACT / RELEASE
mag_test stop               停止连续输出 / 事件
```

磁力计服务在启动时自动初始化，并通过 16 帧采样建立基线。基线未就绪时
`mag_service_get_features()` 返回 -1，`mag_test` 会打印 baseline pending。
默认分类阈值只是占位值，需要根据 `mag_test 20` 的 dR/dL/diff 实测数据调整
`mag_classifier.h` 里的 `MAG_PROFILE_DEFAULT()`。

## imu_test

```text
imu_test id                 读 WHO_AM_I，期望 0x6A
imu_test once               读一帧 accel / gyro / temp
imu_test stream [ms]        连续打印，默认 100 ms
imu_test gesture            打印 shake / tilt / flip 事件
imu_test stop               停止 stream / gesture
imu_test reg <addr>         读一个寄存器
imu_test wreg <addr> <val>  写一个寄存器（调试用）
```

IMU 服务在启动时自动初始化；`stream` 和 `gesture` 只是订阅快照 / 事件，
不会额外占用 I2C。手势参数与 Mosaico 一致：40 ms 采样、600 ms 冷却。

## 注意

`tests/` 目前默认参与编译，方便现场调试。  
release 版本建议用 Kconfig（如 `CONFIG_APP_TEST_HW`）裁掉，或把 shell 命令放到独立的调试固件里。
