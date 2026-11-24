#pragma once

#define NRF_SPI_HOST             SPI2_HOST
#define NRF_SCK                  49
#define NRF_MOSI                 50
#define NRF_MISO                 48
#define NRF_CE                   23
#define NRF_CSN                  51
#define NRF_IRQ                  16
#define NRF_SPI_HZ               4000000
#define NRF_RF_CHANNEL           76u
#define NRF_PAYLOAD_SIZE         32u
#define NRF_ADDR_SIZE            5u
#define NRF_TADDR                "CAMRA"
#define NRF_RADDR                "CTROL"
#define NRF_RETRY_SETUP          0x5Fu
#define NRF_POWER_ON_WAIT_MS     100u
#define NRF_POWER_UP_WAIT_US     1500u

#define FRAME_HEADER_SIZE        4u
#define FRAME_FRAGMENT_SIZE      (NRF_PAYLOAD_SIZE - FRAME_HEADER_SIZE)
#define FRAME_MAX_PACKETS        255u
#define FRAME_MAX_SIZE           (FRAME_FRAGMENT_SIZE * FRAME_MAX_PACKETS)
#define FRAME_TIMEOUT_US         100000u

#define IMAGE_WIDTH              320u
#define IMAGE_HEIGHT             240u
#define IMAGE_RGB565_SIZE        (IMAGE_WIDTH * IMAGE_HEIGHT * 2u)
#define JPEG_DMA_ALIGNMENT       128u
#define JPEG_DECODE_TIMEOUT_MS   50

#define PANEL_WIDTH              640u
#define PANEL_HEIGHT             480u
#define PANEL_CONTROLLER         "NV3051"
#define PANEL_MAX_DSI_LANES      2u

// 拿到面板时序和初始化序列后，在此定义 P4_NV3051_CONFIG_HEADER。
