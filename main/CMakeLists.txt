idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES dht11 ldr oled_menu data_logger serial_cmd
             driver freertos esp_timer
)
