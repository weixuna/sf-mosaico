# apps

业务应用层。

每个子目录是一个应用，负责：

- 应用生命周期：`on_start / on_resume / on_pause / on_stop`
- 应用状态、定时器、数据流
- 应用自己的 LVGL 页面和控件
- 调用 `src/components/services/` 的公共 API

不在这里：

- 板级引脚、电源、寄存器
- 传感器/音频/电源服务实现（放 `src/components/services/`）

当前应用：

```text
src/components/apps/
├── home/           Paper & Earth 主页（由 html-mosaico-ui 移植）
└── rotation3d/     3D 演示（可选）
```

自带表盘 `clock/`、主菜单 `mainmenu/` 和共享 `ui/` 已删除。新 UI 先放在 app
内，出现多个 app 共用的控件时再单独建 `src/components/ui/`。
