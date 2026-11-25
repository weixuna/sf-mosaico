# src/main

应用入口和运行时装配。

- `main.c`：RT-Thread 入口、DFS/模块加载等 SDK 级初始化。
- `app_main.c`：产品入口、LVGL 线程、资源初始化、`gui_app_run`、主循环。
- `blobs/`：`payment_bin.c`、`wf_dig_bin.c` 等 XIP 模块数组。

规则：

- 只做初始化和装配，不写业务逻辑。
- 不直接操作传感器/音频/显示寄存器。
- 业务状态放 `src/components/apps/`，系统服务放 `src/components/services/`。
