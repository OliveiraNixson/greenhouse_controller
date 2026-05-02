idf_component_register(
    SRCS "data_logger.c"
    INCLUDE_DIRS "include"
    REQUIRES spiffs esp_timer oled_menu
)
