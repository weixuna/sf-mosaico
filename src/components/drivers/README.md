# drivers

通用器件驱动层。

这里放“只和芯片本身/总线协议有关”的代码。换一块板，只要总线能通，这些驱动应该不用改。

## 不属于这里的东西

| 内容 | 应该放 |
|---|---|
| 本板引脚、电源轨、复位时序 | `src/board/` |
| I2C/SPI 总线初始化、引脚复用 | `src/board/` |
| 传感器采样线程、滤波、状态机 | `src/components/services/` |
| LVGL 控件、页面 | `src/components/apps/<app>/` |
| 业务逻辑、应用生命周期 | `src/components/apps/` / `src/main/app_main.c` |

判断标准：

> **换一块板，代码需要改吗？**
> - 不需要 → `src/components/drivers/`
> - 需要改引脚/电源/复位 → `src/board/`

## 当前内容

```text
src/components/drivers/
├── io_expander/
│   └── tca9555.c/h      TCA9555 通用驱动，不依赖 RT-Thread
├── charger/
│   └── aw32001.c/h      AW32001 充电管理
├── imu/
│   ├── lsm6ds3tr_c.c/h  LSM6DS3TR-C 六轴 IMU
│   └── lsm6ds3tr_c_regs.h
└── mag/
    └── bmm150.c/h       BMM150 三轴磁力计
```

说明：

- `tca9555.c` 是推荐写法：驱动本身只做寄存器读写，I2C 适配放在
  `src/board/io/tca9555_port.c`，板级引脚映射放在
  `src/board/io/board_io.c`。
- `aw32001.c` 通过 `aw32001_bus_t`（read/write/board_init）访问总线，
  板级适配在 `src/board/power/aw32001_port.c`，驱动本身不 include
  `i2c2.h` 或 `board_io.h`。
- `lsm6ds3tr_c.c` 同样通过 `lsm6ds3tr_c_bus_t` 访问总线，板级适配在
  `src/board/io/lsm6ds3tr_c_port.c`；采样线程和手势在
  `src/components/services/sensors/`。
- `bmm150.c` 通过 `bmm150_bus_t` 访问总线，read/write/map 回调带地址，
  一颗总线可以服务 0x11/0x12 两个实例；板级适配在
  `src/board/io/bmm150_port.c`。

## 驱动目录模板

```text
src/components/drivers/<类别>/<chip>/
├── <chip>.c
├── <chip>.h
└── README.md       可选
```

公共头文件只暴露芯片级 API，例如：

```c
typedef struct
{
    int (*read)(uint8_t reg, uint8_t *buf, uint16_t len);
    int (*write)(uint8_t reg, const uint8_t *buf, uint16_t len);
} xxx_bus_t;

int xxx_init(const xxx_bus_t *bus);
int xxx_read(xxx_data_t *out);
int xxx_set_mode(xxx_mode_t mode);
```

不要把 `rt_device_t`、引脚号、TCA9555 通道号直接暴露给上层。

## 总线访问

推荐两种方式：

1. **port 回调**：像 `tca9555_port.c` 一样，驱动只依赖 `read/write` 函数指针。
2. **RT-Thread I2C 设备**：驱动接收 `struct rt_i2c_bus_device *`，用
   `rt_i2c_transfer()` 访问。

不要在每个驱动里自己初始化 I2C、改引脚、拉电源。

## 电源和复位

驱动不直接操作 TCA9555 或 GPIO。需要上电/复位时：

- 由 `src/components/services/` 或 `src/board/` 在调用 `xxx_init()` 之前完成；
- 或者通过 init 参数传入 `power_on/power_off/reset` 回调。

这样同一颗芯片换到别的板子时，驱动不用改。

## 新增驱动

1. 在 `src/components/drivers/<类别>/` 下新建 `<chip>.c` / `<chip>.h`。
2. 在 `src/components/drivers/SConscript` 的类别列表里加入新目录，或直接放入已有类别目录。
3. 如果芯片需要本板 I2C/电源适配，在 `src/board/` 下加对应的 port 文件。
4. 需要裁剪时，在 `Kconfig` 里加 `CONFIG_DRIVER_<CHIP>`。
5. 加一个 shell 自测命令，例如 `xxx_test`，放在 `tests/` 或驱动文件末尾。

## 约束

- 不 include `lvgl.h`。
- 不 include `app_main.h`、不调用 UI。
- 不在驱动里创建线程、定时器；线程/状态机放 `src/components/services/`。
- 不在 ISR 里做 I2C/SPI 传输。
- 日志用 `LOG_I/W/E`，不要用 `rt_kprintf` 刷屏。
- 初始化失败返回负错误码，不要 `RT_ASSERT` 崩机。
