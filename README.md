# P4-Recv

接收 [RP2040-Data-Trans](https://github.com/lizipear2403/RP2040-Data-Trans) 经单路 nRF24L01+ 发送的 JPEG。收到一帧的全部分片后，交由 ESP32-P4 硬件解码为 320×240 RGB565。接收端面板为 640×480，驱动 IC 为 NV3051，采用 30 针排线连接 MIPI DSI。启用面板后，画面按原尺寸在屏上居中显示，左上角为 (160, 120)。

排线的针序、针距、触点朝向以及 DSI 数据 lane 数以实际面板规格书和板卡原理图为准。

```sh
# 进入已导出 ESP-IDF v6.1.1 的环境
idf.py build
idf.py -p <PORT> flash monitor
```

仓库的 `sdkconfig` 沿用旧版 ESP32-P4 芯片修订配置，并启用 PSRAM。刷写前请按板上芯片修订核对 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3`。无线接线以 `main/pinout.h` 为准，默认信道 76（2476 MHz），应与发送端成对修改。P4 按 mirf 地址语义，经 pipe1 的 `CTROL` 接收并发回 ACK。

NV3051 的初始化序列尚未取得，默认固件保持 MIPI DSI 关闭，可通过 `已解码` 日志联调无线收包和 JPEG 解码。取得该面板的时序和初始化资料后，在 `main/pinout.h` 定义 `P4_NV3051_CONFIG_HEADER`，指向面板配置头文件。该配置需提供：

- `panel_bus` 和 `panel_dpi`，类型分别为 `esp_lcd_dsi_bus_config_t` 和 `esp_lcd_dpi_panel_config_t`。按屏规格设置 lane 数、速率、像素时钟、同步脉宽、前后沿和 DSI 输出色彩格式。代码设置 640×480 有效区、RGB565 输入和单个 DPI framebuffer。
- `panel_prepare(void)`，按板卡设计准备 DSI PHY 电源和面板复位。
- `panel_init(esp_lcd_panel_io_handle_t io)`，通过 DBI 下发实际驱动 IC 的初始化命令。
- `panel_enable_backlight(void)`，在 DPI 启动后开启背光。

上述三个函数返回 `esp_err_t`。面板成功初始化后，日志会区分 `已解码` 与 `已写入屏幕缓冲`。
