# board

本板专属的板级支持代码（BSP）。

这里只放“换一块板就要改”的东西：引脚、电源轨、复位时序、总线、显示/触摸初始化。
通用器件驱动放 `src/components/drivers/`，系统服务放 `src/components/services/`。

## 目录

```text
src/board/
├── display/          LCD/TP 上电、复位、180° 旋转、LVGL 初始化
│   ├── board_display.c
│   └── board_display.h
├── io/               TCA9555 板级映射 + I2C 适配 + IMU/MAG 适配
│   ├── board_io.c
│   ├── board_io.h
│   ├── tca9555_port.c
│   ├── tca9555_port.h
│   ├── lsm6ds3tr_c_port.c
│   ├── lsm6ds3tr_c_port.h
│   ├── bmm150_port.c
│   └── bmm150_port.h
├── bus/              共享 I2C2 总线
│   ├── i2c2.c
│   └── i2c2.h
├── power/            板级上电、LCD 复位、AW32001 适配
│   ├── board_power.c
│   ├── board_power.h
│   ├── aw32001_port.c
│   └── aw32001_port.h
├── audio/            PA/MIC 电源、codec 引脚（有代码时再建）
└── SConscript
```

空目录先不建，等有实际代码再加。

## 各目录职责

### display/

- `board_display_init()`：初始化 LVGL 显示/触摸，并应用本板的物理方向
  （当前是 180°：LCDC V mirror + CO5300 MX）。
- `board_display_resume()`：`RTGRAPHIC_CTRL_POWERON` 重新初始化面板/LCDC 后，
  重新对齐旋转状态。
- 不在这里创建 UI，也不处理业务逻辑。

### io/

- `board_io.c/h`：TCA9555 的 P00~P17 到板级功能的映射：
  LCD/TP 电源、复位、音频 PA、MIC 电源、马达、WiFi、传感器电源等。
- `tca9555_port.c/h`：TCA9555 通用驱动到 I2C2 的适配。
- `lsm6ds3tr_c_port.c/h`：LSM6DS3TR-C 的 I2C2 读写、SENS_3V3 上电和轴映射。
- `bmm150_port.c/h`：两颗 BMM150（0x11/0x12）的 I2C2 读写、SENS_3V3 上电和独立轴映射。
- 其他模块通过 `board_io_set()` / `board_io_get()` 访问，不直接碰 TCA9555 寄存器。

### bus/

- `i2c2.c/h`：PA24/PA25 上的共享 I2C2 总线。
- TCA9555、AW32001、LSM6DS3TR-C、BMM150 都走这条总线。
- 总线互斥/超时在这里统一处理，器件驱动不各自初始化 I2C。

### power/

- `board_power.c/h`：板级上电、LCD 复位时序。
- `aw32001_port.c/h`：AW32001 的 I2C2 读写、SENS_3V3 电源轨适配。

## 规则

- 不 include `lvgl.h`，不创建 LVGL 控件。
- 不包含业务逻辑、应用生命周期、页面跳转。
- 不直接访问芯片寄存器；芯片操作放 `src/components/drivers/`。
- 电源/复位统一走 `board_io`，不要在驱动里直接拉 GPIO。
- 新增一个板级功能时，新建一个子目录（如 `power/`、`audio/`），
  `SConscript` 会自动编译并把目录加入 include path。

## 多板型

目前只有一块板，所以 `src/board/` 直接放功能目录。

如果以后有多个 SKU，再增加板级子目录：

```text
src/board/
├── board_a/
│   ├── display/
│   ├── io/
│   └── power/
└── board_b/
    ├── display/
    ├── io/
    └── power/
```

公共代码不要放 `src/board/`，放 `src/components/drivers/` 或 `src/components/services/`。
