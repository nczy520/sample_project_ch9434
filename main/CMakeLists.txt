idf_component_register(
    SRCS "main.c"
         "ch9434_spi.c"
         "ch9434_drv.c"
         "ch9434_uart.c"
         "test_app.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_spi esp_timer
)
