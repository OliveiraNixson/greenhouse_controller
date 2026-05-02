idf_component_register(
    SRCS "serial_cmd.c"
    INCLUDE_DIRS "include"
    REQUIRES driver oled_menu data_logger
)
