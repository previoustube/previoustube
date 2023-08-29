idf_component_register(
    SRCS "pcf8563.c" "i2c_helper.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)