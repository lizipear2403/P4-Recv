/* 从 RP2040 接收定长 FramePkt，分片齐全且通过 JPEG 解码后才刷新画面。 */
#include "display.h"
#include "frame.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mirf.h"
#include "sdkconfig.h"

static const char *const TAG = "P4-Recv";
static frame_assembly_t frame;
static uint32_t dropped_frames;

static void radio_init(NRF24_t *radio)
{
    uint8_t receive_address[] = NRF_RADDR;
    uint8_t transmit_address[] = NRF_TADDR;
    _Static_assert(sizeof(receive_address) == mirf_ADDR_LEN + 1 &&
                   sizeof(transmit_address) == mirf_ADDR_LEN + 1, "Invalid nRF24 address width");

    Nrf24_init(radio);
    vTaskDelay(pdMS_TO_TICKS(NRF_POWER_ON_WAIT_MS));
    Nrf24_powerDown(radio); // mirf 在 CONFIG 中选择 1 字节 CRC。
    Nrf24_configRegister(radio, SETUP_AW, 0x03);
    Nrf24_configRegister(radio, EN_AA, (1 << ENAA_P0) | (1 << ENAA_P1));
    Nrf24_configRegister(radio, EN_RXADDR, 1 << ERX_P1);
    Nrf24_configRegister(radio, DYNPD, 0);
    Nrf24_configRegister(radio, FEATURE, 0);
    Nrf24_SetOutputRF_PWR(radio, RF24_PA_MAX);
    Nrf24_SetSpeedDataRates(radio, RF24_2MBPS);
    Nrf24_configRegister(radio, SETUP_RETR, NRF_RETRY_SETUP);
    ESP_ERROR_CHECK(Nrf24_setRADDR(radio, receive_address));
    ESP_ERROR_CHECK(Nrf24_setTADDR(radio, transmit_address));
    Nrf24_config(radio, CONFIG_RADIO_CHANNEL, NRF_PAYLOAD_SIZE);
}

static void drop_frame(const char *reason)
{
    frame.active = false;
    ESP_LOGW(TAG, "帧 %u 丢弃 (%u/%u 包，累计 %u)：%s",
             frame.id, frame.received, frame.total, (unsigned)++dropped_frames, reason);
}

static void accept_packet(const uint8_t packet[NRF_PAYLOAD_SIZE], uint64_t now)
{
    frame_header_t header;
    if (!frame_read_header(packet, &header)) {
        ESP_LOGW(TAG, "分片编号无效：帧 %u，分片 %u/%u", header.id, header.index, header.total);
        return;
    }
    if (frame.known_id && frame.id == header.id) {
        if (!frame.active) {
            return;
        }
        if (frame.total != header.total) {
            drop_frame("总包数不一致");
            return;
        }
    } else {
        if (frame.active) {
            drop_frame("下一帧已到，当前帧分片不全");
        }
        frame_start(&frame, header);
    }
    if (!frame_add(&frame, header, packet + FRAME_HEADER_SIZE, now)) {
        return;
    }

    const size_t len = frame_jpeg_size(&frame);
    if (!len) {
        drop_frame("JPEG 首尾标记或尾包填充无效");
        return;
    }
    const esp_err_t err = display_jpeg(frame.jpeg, len);
    if (err != ESP_OK) {
        drop_frame(esp_err_to_name(err));
        return;
    }
    frame.active = false;
    ESP_LOGI(TAG, "帧 %u %u 包，JPEG %u B，累计丢帧 %u，%s",
             frame.id, frame.received, (unsigned)len, (unsigned)dropped_frames,
             display_has_panel() ? "已写入屏幕缓冲" : "已解码");
}

void app_main(void)
{
    ESP_ERROR_CHECK(display_init());
    NRF24_t radio = {0};
    radio_init(&radio);
    ESP_LOGI(TAG, "nRF24 就绪，信道 %u，等待 RP2040 发帧", (unsigned)CONFIG_RADIO_CHANNEL);

    uint8_t packet[NRF_PAYLOAD_SIZE] = {0};
    while (true) {
        const uint64_t now = esp_timer_get_time();
        if (frame.active && now - frame.last_fragment_us >= FRAME_TIMEOUT_US) {
            drop_frame("等待分片超时");
        }
        // mirf 读包后会清 RX_DR，所以检查 FIFO 状态。
        if (!Nrf24_rxFifoEmpty(&radio)) {
            Nrf24_getData(&radio, packet);
            accept_packet(packet, now);
        } else {
            vTaskDelay(1);
        }
    }
}
