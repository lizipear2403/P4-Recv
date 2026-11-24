idf_component_register(SRCS "P4-Recv.c" "nrf24.c" "frame.c" "display.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_driver_jpeg
                                     esp_lcd esp_timer esp_psram)
