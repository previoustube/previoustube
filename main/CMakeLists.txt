idf_component_register(
        SRCS
        "main.c"
        "leds.c"
        "leds.h"
        "lcds.c"
        "wifi.cpp"
        "touchpads.c"
        "gui.c"
        "fonts/oswald_100.c"
        "fonts/oswald_120.c"
        INCLUDE_DIRS
        "."
)

spiffs_create_partition_image(spiffs spiffs_assets FLASH_IN_PROJECT)
idf_build_set_property(COMPILE_OPTIONS "-DLV_LVGL_H_INCLUDE_SIMPLE" APPEND)
