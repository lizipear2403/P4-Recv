#include "Mask.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "mirf.h"
#include "soc/gpio_num.h"
#include <assert.h>
#include <esp_log.h>
#include <esp_system.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SPI_SCK 49
#define SPI_MOSI 50
#define SPI_MISO 48

#define NRF_IRQ 16

// static const char *RADDR = "CTROL";
// static const char *TADDR = "CAMRA";

static uint8_t buf[32] = {0};

static uint8_t pic_buf[2 << 14] = {0};

static uint8_t cnt = 0;

static esp_err_t ret = {0};

// frame_id : 10min -> 10fps -> 6000 -> 2^8 * 2 ^ 8 = 16384
// sub_frame_id : 1 frame = 6000 byte = 2^8 = 512
// frame_id         = pkt_info & 0000_0000_0000_0000_1111_1111_1111_1111
// sub_frame_id     = pkt_info & 0000_0000_1111_1111_0000_0000_0000_0000
// expect_pkt_num   = pkt_info & 1111_1111_0000_0000_0000_0000_0000_0000
//

struct FramePkt {
  uint32_t pkt_info;    // 4
  uint8_t pkt_data[28]; // 28
};

struct CtrlPkt {
  uint16_t exp_sub_data; // 2
  uint8_t pkt_type;      // 1
};

typedef struct FramePkt FramePkt;
typedef struct CtrlPkt CtrlPkt;

void nrf_init(NRF24_t *dev, uint8_t NRF_CE_PIN, uint8_t NRF_CSN_PIN,
              uint8_t SPI_HOST) {

  gpio_reset_pin(NRF_CE_PIN);
  gpio_set_direction(NRF_CE_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(NRF_CE_PIN, 0);

  gpio_reset_pin(NRF_CSN_PIN);
  gpio_set_direction(NRF_CSN_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(NRF_CSN_PIN, 0);
  spi_device_interface_config_t nrf_spi = {0};
  nrf_spi.clock_speed_hz = 4 * 1000 * 1000;
  nrf_spi.mode = 0;
  nrf_spi.spics_io_num = -1;
  nrf_spi.queue_size = 7;
  nrf_spi.flags = SPI_DEVICE_NO_DUMMY;

  spi_device_handle_t handle = {0};

  ret = spi_bus_add_device(SPI_HOST, &nrf_spi, &handle);
  assert(ret == ESP_OK);

  dev->_SPIHandle = handle;
  dev->cePin = NRF_CE_PIN;
  dev->csnPin = NRF_CSN_PIN;
  Nrf24_printDetails(dev);
}

static FramePkt pkt_buf = {0};

void print_data(FramePkt *buf) {
  printf("data:");
  for (uint8_t i = 0; i < 28; i++) {
    printf("%zu ", buf->pkt_data[i]);
  }
  putc('\n', stdout);
}

void app_main(void) {

  spi_bus_config_t bus_config = {
      .miso_io_num = SPI_MISO,
      .mosi_io_num = SPI_MOSI,
      .sclk_io_num = SPI_SCK,
      .quadhd_io_num = -1,
      .quadwp_io_num = -1,
  };

  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

  NRF24_t recv = {0};
  nrf_init(&recv, GPIO_NUM_23, 51, SPI2_HOST);
  Nrf24_config(&recv, 90, 32);
  Nrf24_setRADDR(&recv, (uint8_t *)"CTROL");
  Nrf24_setTADDR(&recv, (uint8_t *)"CAMRA");

  Nrf24_powerUpRx(&recv);
  gpio_set_level(GPIO_NUM_4, 1);

  while (1) {
    while (!Nrf24_rxFifoEmpty(&recv)) {
      Nrf24_getData(&recv, (uint8_t *)&pkt_buf);
      // printf("EXP_PKT_NUM : %#lx , SUB_FRAME_ID : %#lx , FRAME_ID : %#lx \n",
      //        (pkt_buf.pkt_info & EXPECT_PKT_NUM),
      //        (pkt_buf.pkt_info & SUB_FRAME_MASK) << 8,
      //        (pkt_buf.pkt_info & FRAME_ID_MASK) << 16);
      // print_data(b);
      // printf("pkt_id : %d \n", (int)b->pkt_data[]);
      printf("INFO : %#lx \n", pkt_buf.pkt_info);
      memcpy(&pic_buf[(uint8_t)((pkt_buf.pkt_info & SUB_FRAME_MASK) >> 16)],
             pkt_buf.pkt_data, 28);
      memset(&pkt_buf, 0, sizeof(pkt_buf));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
