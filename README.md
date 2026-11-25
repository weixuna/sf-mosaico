# 简介后面再写


# 硬件接口与引脚定义

## 1. GPIO 扩展芯片：TCA9555RTWR（UMW）

### 1.1 MCU 与 TCA9555 I²C 连接

| 信号 | MCU 引脚 |
|---|---|
| SCL | PA24 |
| SDA | PA25 |
| INT | PA41 |

### 1.2 TCA9555 引脚分配

| 引脚编号 | 芯片引脚 | 网络名称 | 功能说明 |
|---|---|---|---|
| 1 | P00 | 2.4G_RFSW | 57X 与 WiFi 的 2.4G 天线切换控制；高电平切换到 57X 的 2.4G 天线，低电平切换到 WiFi 的 2.4G 天线 |
| 2 | P01 | WF_PWRKEY | WiFi 开关键输入 |
| 3 | P02 | TF_DET | TF 卡插入检测 |
| 4 | P03 | HST_WAK_WF | Host Wake WiFi 输入 |
| 5 | P04 | AUDIO_PA_EN | NS4150B 功放使能控制（CTRL） |
| 6 | P05 | MIC_PWR_EN | MIC LDO 电源使能 |
| 7 | P06 | WF_PWR_EN | WiFi LDO 电源使能 |
| 8 | P07 | SENS_PWR_EN | 传感器 LDO 电源使能 |
| 10 | P10 | NC | 未连接 |
| 11 | P11 | NC | 未连接 |
| 12 | P12 | BOOST_EN | 5V 升压使能 |
| 13 | P13 | VCI_EN | LCD 电源使能 |
| 14 | P14 | TP_RST | 触摸屏复位 |
| 15 | P15 | MOTOR_EN | 马达使能 |
| 16 | P16 | RESET | LCD 复位 |
| 17 | P17 | LCD_PWR_EN | LCD LDO 电源使能 |

### 1.3 TCA9555 地址及电源引脚

| 引脚编号 | 芯片引脚 | 连接 |
|---|---|---|
| 18 | A0 | GND |
| 23 | A1 | GND |
| 24 | A2 | GND |
| 21 | VCC | 电源 |
| 9 | GND | GND |
| 25 | EP | GND |
| 22 | INT# | IO_INT（10kΩ 上拉） |

---

## 2. AMOLED 显示屏及触摸屏

### 2.1 MCU GPIO 引脚分配

| MCU 引脚 | 网络名称 | 功能说明 |
|---|---|---|
| PA45 | TP_INT | 触摸中断 |
| PA46 | TP_SDA | 触摸 I²C 数据 |
| PA50 | TP_SCL | 触摸 I²C 时钟 |
| PA51 | LCD_QSPI_CS | LCD QSPI 片选 |
| PA47 | LCD_QSPI_D0 | LCD QSPI 数据 0 |
| PA52 | LCD_QSPI_D1 | LCD QSPI 数据 1 |
| PA48 | LCD_QSPI_CLK | LCD QSPI 时钟 |
| PA53 | LCD_QSPI_D3 | LCD QSPI 数据 3 |
| PA49 | LCD_QSPI_D2 | LCD QSPI 数据 2 |
| PA54 | LCD_TE | LCD TE 同步信号 |

### 2.2 显示屏控制信号

| 网络名称 | 控制来源 | 功能 |
|---|---|---|
| VCI_EN | TCA9555 P13 | LCD 电源使能 |
| LCD_PWR_EN | TCA9555 P17 | LCD LDO 电源使能 |
| RESET | TCA9555 P16 | LCD 复位 |
| LCD_TE | PA54 | LCD TE 同步信号 |
| TP_RST | TCA9555 P14 | 触摸屏复位 |
| TP_INT | PA45 | 触摸中断 |

---

## 3. I²C 外设及传感器

### 3.1 I²C 总线分配

多个外设共用 MCU 的 I²C 总线。

| 信号 | MCU 引脚 |
|---|---|
| SCL | PA24 |
| SDA | PA25 |

### 3.2 AW32001ECSR

| 参数 | 配置 |
|---|---|
| 芯片型号 | AW32001ECSR |
| SCL | PA24 |
| SDA | PA25 |
| INT | 未连接 |

### 3.3 LSM6DS3TR-C（六轴 IMU）

| 参数 | 配置 |
|---|---|
| 芯片型号 | LSM6DS3TR-C |
| I²C 地址 | 0x6A |
| SCL | PA24 |
| SDA | PA25 |
| INT1 | PA26 |

### 3.4 BMM150 磁力计 1

| 参数 | 配置 |
|---|---|
| 芯片型号 | BMM150 |
| I²C 地址 | 0x11 |
| SCL | PA24 |
| SDA | PA25 |
| INT | PA20 |

### 3.5 BMM150 磁力计 2

| 参数 | 配置 |
|---|---|
| 芯片型号 | BMM150 |
| I²C 地址 | 0x12 |
| SCL | PA24 |
| SDA | PA25 |
| INT | PA27 |

---

## 4. USB 接口模块

### 4.1 USB GPIO 引脚分配

| MCU 引脚 | 网络名称 | 功能说明 |
|---|---|---|
| PA35 | USB_DP | USB 差分数据正端 |
| PA36 | USB_DM | USB 差分数据负端 |

### 4.2 USB 芯片

| 模块 | 芯片型号 | 功能说明 |
|---|---|---|
| USB HUB | CH334P | USB 集线器，用于扩展 USB 接口 |
| USB 转串口 | CH343P | USB-UART 桥接芯片，用于 USB 与 UART 串口通信 |

---

## 5. UART 串口接口

### 5.1 MCU GPIO 引脚分配

| MCU 引脚 | 网络名称 | 功能说明 |
|---|---|---|
| PA18 | UART_RX | UART 串口接收 |
| PA19 | UART_TX | UART 串口发送 |

### 5.2 UART 芯片

| 项目 | 配置 |
|---|---|
| 芯片型号 | CH343P |
| 通信接口 | USB-UART |
| UART_RX | PA18 |
| UART_TX | PA19 |

---

## 6. 电源管理模块

### 6.1 DC-DC 电源芯片

| 模块 | 芯片型号 | 功能 |
|---|---|---|
| MCU DC-DC | ETA3425D2G | MCU 电源转换 |
| WiFi DC-DC | ETA3425D2G | WiFi 模块电源转换 |

### 6.2 电源控制 GPIO

| 网络名称 | 控制来源 | 功能 |
|---|---|---|
| BOOST_EN | TCA9555 P12 | 5V 升压使能 |
| VCI_EN | TCA9555 P13 | LCD 电源使能 |
| LCD_PWR_EN | TCA9555 P17 | LCD LDO 电源使能 |
| WF_PWR_EN | TCA9555 P06 | WiFi LDO 电源使能 |
| MIC_PWR_EN | TCA9555 P05 | MIC LDO 电源使能 |
| SENS_PWR_EN | TCA9555 P07 | 传感器 LDO 电源使能 |

---

## 7. 音频模块

### 7.1 功率放大器（PA）

| 参数 | 配置 |
|---|---|
| 芯片型号 | NS4150B |
| 使能网络 | AUDIO_PA_EN |
| 控制来源 | TCA9555 P04 |
| 控制引脚 | CTRL |

### 7.2 数字麦克风（MIC）

| 参数 | 配置 |
|---|---|
| 麦克风型号 | MSM261DCB002 |
| 数据接口 | PDM |
| PDM_DATA | PA56 |
| PDM_CLK | PA55 |
| 电源控制 | MIC_PWR_EN |
| 控制来源 | TCA9555 P05 |

---

## 8. 振动马达

| 参数 | 配置 |
|---|---|
| 马达型号 | Z3OC1T8219731 |
| 使能网络 | MOTOR_EN |
| 控制来源 | TCA9555 P15 |

---

## 9. WiFi 模块控制

### 9.1 WiFi 控制信号

| 网络名称 | 控制来源 | 功能说明 |
|---|---|---|
| HST_WAK_WF | TCA9555 P03 | Host Wake WiFi 输入 |
| WF_PWRKEY | TCA9555 P01 | WiFi 开关键输入 |
| WF_PWR_EN | TCA9555 P06 | WiFi LDO 电源使能 |
| 2.4G_RFSW | TCA9555 P00 | 57X 与 WiFi 的 2.4G 天线切换；高电平选择 57X，低电平选择 WiFi |

---

## 10. 芯片型号汇总

| 序号 | 模块 | 芯片型号 |
|---|---|---|
| 1 | GPIO 扩展 | TCA9555RTWR（UMW） |
| 2 | USB HUB | CH334P |
| 3 | USB 转串口 | CH343P |
| 4 | MCU DC-DC | ETA3425D2G |
| 5 | WiFi DC-DC | ETA3425D2G |
| 6 | 电源管理 | AW32001ECSR |
| 7 | 六轴 IMU | LSM6DS3TR-C |
| 8 | 磁力计 1 | BMM150 |
| 9 | 磁力计 2 | BMM150 |
| 10 | 功率放大器 | NS4150B |
| 11 | 数字麦克风 | MSM261DCB002 |
| 12 | 振动马达 | Z3OC1T8219731 |

---

## 11. MCU GPIO 引脚总表

| MCU 引脚 | 网络名称 | 所属模块 |
|---|---|---|
| PA18 | UART_RX | UART |
| PA19 | UART_TX | UART |
| PA20 | BMM150_INT1 | 磁力计 1 |
| PA24 | I2C_SCL | I²C 总线 |
| PA25 | I2C_SDA | I²C 总线 |
| PA26 | LSM6DS3_INT1 | 六轴 IMU |
| PA27 | BMM150_INT2 | 磁力计 2 |
| PA35 | USB_DP | USB |
| PA36 | USB_DM | USB |
| PA41 | IO_INT | TCA9555 |
| PA45 | TP_INT | 触摸屏 |
| PA46 | TP_SDA | 触摸屏 |
| PA47 | LCD_QSPI_D0 | AMOLED |
| PA48 | LCD_QSPI_CLK | AMOLED |
| PA49 | LCD_QSPI_D2 | AMOLED |
| PA50 | TP_SCL | 触摸屏 |
| PA51 | LCD_QSPI_CS | AMOLED |
| PA52 | LCD_QSPI_D1 | AMOLED |
| PA53 | LCD_QSPI_D3 | AMOLED |
| PA54 | LCD_TE | AMOLED |
| PA55 | PDM_CLK | 数字麦克风 |
| PA56 | PDM_DATA | 数字麦克风 |
