#include "display.h"
#include "pinout.h"

#include "driver/jpeg_decode.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef P4_NV3051_CONFIG_HEADER
#include P4_NV3051_CONFIG_HEADER
#endif

_Static_assert(PANEL_WIDTH >= IMAGE_WIDTH && PANEL_HEIGHT >= IMAGE_HEIGHT,
               "LCD must fit the decoded image");

static const char *const TAG = "display";
static jpeg_decoder_handle_t decoder;
static esp_lcd_panel_handle_t panel;
static EXT_RAM_BSS_ATTR _Alignas(JPEG_DMA_ALIGNMENT) uint8_t pixels[IMAGE_RGB565_SIZE];

#ifdef P4_NV3051_CONFIG_HEADER
static esp_err_t display_init_panel(void)
{
    esp_lcd_dpi_panel_config_t dpi_config = panel_dpi;
    dpi_config.video_timing.h_size = PANEL_WIDTH;
    dpi_config.video_timing.v_size = PANEL_HEIGHT;
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.num_fbs = 1;
    const esp_lcd_dbi_io_config_t dbi = {
        .virtual_channel = dpi_config.virtual_channel,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (!panel_bus.num_data_lanes || panel_bus.num_data_lanes > PANEL_MAX_DSI_LANES ||
        panel_bus.lane_bit_rate_mbps <= 0 || dpi_config.dpi_clock_freq_mhz <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_lcd_dsi_bus_handle_t bus;
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(panel_prepare(), TAG, "面板电源或复位初始化失败");
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&panel_bus, &bus), TAG, "DSI 初始化失败");
    esp_err_t err = esp_lcd_new_panel_io_dbi(bus, &dbi, &io);
    if (err != ESP_OK) {
        esp_lcd_del_dsi_bus(bus);
        return err;
    }

    esp_lcd_panel_handle_t dpi = NULL;
    err = panel_init(io);
    if (err == ESP_OK) {
        err = esp_lcd_new_panel_dpi(bus, &dpi_config, &dpi);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(dpi);
    }
    if (err == ESP_OK) {
        err = panel_enable_backlight();
    }
    if (err != ESP_OK) {
        if (dpi) {
            esp_lcd_panel_del(dpi);
        }
        esp_lcd_panel_io_del(io);
        esp_lcd_del_dsi_bus(bus);
        return err;
    }
    panel = dpi;
    return ESP_OK;
}
#endif

esp_err_t display_init(void)
{
    const jpeg_decode_engine_cfg_t cfg = {.timeout_ms = JPEG_DECODE_TIMEOUT_MS};
    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&cfg, &decoder), TAG, "JPEG 引擎初始化失败");
#ifdef P4_NV3051_CONFIG_HEADER
    const esp_err_t err = display_init_panel();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, PANEL_CONTROLLER " 初始化失败，继续解码：%s", esp_err_to_name(err));
    }
#else
    ESP_LOGI(TAG, PANEL_CONTROLLER " %u×%u 等待面板时序和初始化序列，运行收包和 JPEG 解码",
             PANEL_WIDTH, PANEL_HEIGHT);
#endif
    return ESP_OK;
}

bool display_has_panel(void)
{
    return panel != NULL;
}

esp_err_t display_jpeg(const uint8_t *jpeg, size_t len)
{
    jpeg_decode_picture_info_t info;
    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(jpeg, len, &info), TAG, "JPEG 头无效");
    if (info.width != IMAGE_WIDTH || info.height != IMAGE_HEIGHT) {
        ESP_LOGW(TAG, "JPEG 尺寸不符 %u×%u", (unsigned)info.width, (unsigned)info.height);
        return ESP_ERR_INVALID_SIZE;
    }
    const jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        // DPI framebuffer 按本机字节序读取 RGB565。
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t output_size;
    // QVGA 的宽、高均为 16 的倍数，RGB565 无需额外行填充。
    ESP_RETURN_ON_ERROR(jpeg_decoder_process(decoder, &cfg, jpeg, len, pixels, sizeof(pixels), &output_size),
                        TAG, "JPEG 解码失败");
    if (output_size != sizeof(pixels)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (panel) {
        const int x = (PANEL_WIDTH - IMAGE_WIDTH) / 2;
        const int y = (PANEL_HEIGHT - IMAGE_HEIGHT) / 2;
        return esp_lcd_panel_draw_bitmap(panel, x, y, x + IMAGE_WIDTH, y + IMAGE_HEIGHT, pixels);
    }
    return ESP_OK;
}
