# src/resources

字体、图片、字符串资源。

```text
src/resources/
├── fonts/
│   ├── bitmap/       LVGL 位图字体
│   └── freetype/     TTF 字体，由构建脚本转换为 C 文件
├── images/
│   ├── common/ezip/
│   ├── common/no_ezip/
│   └── common/large_ezip/
└── strings/          en_us/zh_cn JSON + lang_pack 生成
```

资源由 `src/resources/SConscript` 和各子目录的 `SConscript` 转换、编译并放进固件分区。  
文件名必须是合法 C 标识符：用下划线，不要用连字符、空格或中文字符，否则生成的
C 符号无法编译。

不要把资源转换逻辑写到 `src/components/` 或 `src/board/` 里。
