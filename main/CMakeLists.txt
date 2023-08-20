idf_component_register(
        SRCS
        "clock.cpp"
        "drivers/lcds.cpp"
        "drivers/leds.cpp"
        "drivers/touchpads.cpp"
        "drivers/wifi.cpp"
        "fonts/oswald_40.c"
        "fonts/oswald_60.c"
        "fonts/oswald_100.c"
        "fonts/oswald_120.c"
        "gui.cpp"
        "led_manager.cpp"
        "main.cpp"
        "webserver.cpp"
        "flapper.cpp"
        "spiram_allocate.cpp"
        INCLUDE_DIRS
        "."
)

spiffs_create_partition_image(spiffs spiffs FLASH_IN_PROJECT)
idf_build_set_property(COMPILE_OPTIONS "-DLV_LVGL_H_INCLUDE_SIMPLE" APPEND)
