#include "nrf24.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_rom_sys.h"

enum {
    REG_CONFIG = 0x00, REG_EN_AA = 0x01, REG_EN_RXADDR = 0x02,
    REG_SETUP_AW = 0x03, REG_SETUP_RETR = 0x04, REG_RF_CH = 0x05,
    REG_RF_SETUP = 0x06, REG_STATUS = 0x07, REG_RX_ADDR_P0 = 0x0A,
    REG_RX_ADDR_P1 = 0x0B, REG_TX_ADDR = 0x10, REG_RX_PW_P0 = 0x11,
    REG_RX_PW_P1 = 0x12, REG_FIFO_STATUS = 0x17, REG_DYNPD = 0x1C, REG_FEATURE = 0x1D,
    CMD_WRITE_REG = 0x20, CMD_RX_PAYLOAD = 0x61,
    CMD_FLUSH_TX = 0xE1, CMD_FLUSH_RX = 0xE2,
    CONFIG_EN_CRC = 1u << 3, CONFIG_PWR_UP = 1u << 1, CONFIG_PRIM_RX = 1u << 0,
    STATUS_RX_DR = 1u << 6, STATUS_TX_DS = 1u << 5, STATUS_MAX_RT = 1u << 4,
    FIFO_RX_EMPTY = 1u << 0, PIPE0 = 1u << 0, PIPE1 = 1u << 1,
    ADDR_WIDTH_5_BYTES = 0x03, RF_2MBPS_0DBM = 0x0E,
};

static const char *const TAG = "nrf24";
static spi_device_handle_t spi;

static esp_err_t transfer(uint8_t opcode, const uint8_t *out, uint8_t *in, size_t size)
{
    uint8_t tx[1 + NRF_PAYLOAD_SIZE] = {opcode};
    uint8_t rx[sizeof(tx)];
    if (out) {
        memcpy(tx + 1, out, size);
    }
    spi_transaction_t transaction = {
        .length = (1 + size) * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_polling_transmit(spi, &transaction);
    if (err == ESP_OK && in) {
        memcpy(in, rx + 1, size);
    }
    return err;
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    return transfer(CMD_WRITE_REG | reg, &value, NULL, sizeof(value));
}

esp_err_t nrf24_init(void)
{
    static const uint8_t tx_addr[] = NRF_TADDR;
    static const uint8_t rx_addr[] = NRF_RADDR;
    _Static_assert(sizeof(tx_addr) == NRF_ADDR_SIZE + 1 && sizeof(rx_addr) == NRF_ADDR_SIZE + 1,
                   "nRF24 addresses must have five bytes");
    const spi_bus_config_t bus = {
        .sclk_io_num = NRF_SCK,
        .mosi_io_num = NRF_MOSI,
        .miso_io_num = NRF_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    const spi_device_interface_config_t device = {
        .clock_speed_hz = NRF_SPI_HZ,
        .mode = 0,
        .spics_io_num = NRF_CSN,
        .queue_size = 1,
        .flags = SPI_DEVICE_NO_DUMMY,
    };
    const gpio_config_t ce = {
        .pin_bit_mask = 1ULL << NRF_CE,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&ce), TAG, "CE 配置失败");
    ESP_RETURN_ON_ERROR(gpio_set_level(NRF_CE, 0), TAG, "CE 拉低失败");
    ESP_RETURN_ON_ERROR(spi_bus_initialize(NRF_SPI_HOST, &bus, SPI_DMA_DISABLED), TAG, "SPI 初始化失败");
    ESP_RETURN_ON_ERROR(spi_bus_add_device(NRF_SPI_HOST, &device, &spi), TAG, "CSN 配置失败");
    esp_rom_delay_us(NRF_POWER_ON_WAIT_MS * 1000u);

    // mirf 将本端 RADDR 写在 pipe1。PTX 的 pipe0 用于接收该地址的 ACK。
    static const uint8_t regs[][2] = {
        {REG_CONFIG, CONFIG_EN_CRC | CONFIG_PRIM_RX},
        {REG_EN_AA, PIPE0 | PIPE1}, {REG_EN_RXADDR, PIPE1},
        {REG_SETUP_AW, ADDR_WIDTH_5_BYTES}, {REG_SETUP_RETR, NRF_RETRY_SETUP},
        {REG_RF_CH, NRF_RF_CHANNEL}, {REG_RF_SETUP, RF_2MBPS_0DBM},
        {REG_RX_PW_P0, NRF_PAYLOAD_SIZE}, {REG_RX_PW_P1, NRF_PAYLOAD_SIZE},
        {REG_DYNPD, 0}, {REG_FEATURE, 0},
    };
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        ESP_RETURN_ON_ERROR(write_reg(regs[i][0], regs[i][1]), TAG, "寄存器 0x%02x 写失败", regs[i][0]);
    }
    ESP_RETURN_ON_ERROR(transfer(CMD_WRITE_REG | REG_TX_ADDR, tx_addr, NULL, NRF_ADDR_SIZE), TAG, "TX_ADDR 写失败");
    ESP_RETURN_ON_ERROR(transfer(CMD_WRITE_REG | REG_RX_ADDR_P0, tx_addr, NULL, NRF_ADDR_SIZE), TAG, "RX_ADDR_P0 写失败");
    ESP_RETURN_ON_ERROR(transfer(CMD_WRITE_REG | REG_RX_ADDR_P1, rx_addr, NULL, NRF_ADDR_SIZE), TAG, "RX_ADDR_P1 写失败");
    ESP_RETURN_ON_ERROR(transfer(CMD_FLUSH_RX, NULL, NULL, 0), TAG, "清 RX FIFO 失败");
    ESP_RETURN_ON_ERROR(transfer(CMD_FLUSH_TX, NULL, NULL, 0), TAG, "清 TX FIFO 失败");
    ESP_RETURN_ON_ERROR(write_reg(REG_STATUS, STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT), TAG, "清状态失败");

    const uint8_t config = CONFIG_EN_CRC | CONFIG_PWR_UP | CONFIG_PRIM_RX;
    ESP_RETURN_ON_ERROR(write_reg(REG_CONFIG, config), TAG, "进入 PRX 失败");
    esp_rom_delay_us(NRF_POWER_UP_WAIT_US);
    uint8_t config_read;
    uint8_t channel;
    ESP_RETURN_ON_ERROR(transfer(REG_CONFIG, NULL, &config_read, 1), TAG, "CONFIG 读失败");
    ESP_RETURN_ON_ERROR(transfer(REG_RF_CH, NULL, &channel, 1), TAG, "RF_CH 读失败");
    ESP_RETURN_ON_FALSE(config_read == config && channel == NRF_RF_CHANNEL,
                        ESP_ERR_INVALID_RESPONSE, TAG, "寄存器回读异常 CONFIG=0x%02x RF_CH=%u",
                        config_read, channel);
    return gpio_set_level(NRF_CE, 1);
}

esp_err_t nrf24_receive(uint8_t payload[NRF_PAYLOAD_SIZE], bool *received)
{
    *received = false;
    uint8_t fifo;
    ESP_RETURN_ON_ERROR(transfer(REG_FIFO_STATUS, NULL, &fifo, 1), TAG, "读 FIFO 状态失败");
    if (fifo & FIFO_RX_EMPTY) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(write_reg(REG_STATUS, STATUS_RX_DR), TAG, "清 RX_DR 失败");
    ESP_RETURN_ON_ERROR(transfer(CMD_RX_PAYLOAD, NULL, payload, NRF_PAYLOAD_SIZE), TAG, "读 payload 失败");
    *received = true;
    return ESP_OK;
}
