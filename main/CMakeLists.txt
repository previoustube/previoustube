idf_component_register(
        SRCS
        "main.c"
        "leds.c"
        "leds.h"
        "lcds.c"
        "wifi.cpp"
        "touchpads.c"
        "gui.c"
        INCLUDE_DIRS
        "."
)

spiffs_create_partition_image(spiffs spiffs_assets FLASH_IN_PROJECT)
