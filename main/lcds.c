//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include "lcds.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <rom/ets_sys.h>
#include <stdbool.h>
#include <string.h>

// shared
#define CONFIG_MOSI_GPIO 13
#define CONFIG_SCLK_GPIO 12
#define CONFIG_GPIO_DC 14
#define CONFIG_GPIO_RESET 27
#define CONFIG_GPIO_BL 19

#define CONFIG_WIDTH 80
#define CONFIG_HEIGHT 162
#define CONFIG_OFFSETX 24
#define CONFIG_OFFSETY 0

#define MAX_TRANSFER_SIZE 4094

// unique per display
/*
  LCD1 CS pin     33
  LCD2 CS pin     26
  LCD3 CS pin     21
  LCD4 CS pin     0
  LCD5 CS pin     5
  LCD6 CS pin     18
 */

static const gpio_num_t GPIO_CS_PINS[NUM_LCDS] = {33, 26, 21, 0, 5, 18};
static const char *TAG = "lcds";

static spi_device_handle_t spi_device_handle = NULL;
static bool initialized = false;

static DMA_ATTR uint16_t framebuffer[CONFIG_WIDTH * CONFIG_HEIGHT];

void init_red_tab();
void deselect_all_displays();

void lcds_init() {
  assert(spi_device_handle == NULL);
  assert(!initialized);

  ESP_LOGI(TAG, "Configuring LCD SPI devices");

  for (size_t i = 0; i < NUM_LCDS; i++) {
    gpio_num_t pin = GPIO_CS_PINS[i];
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  }
  deselect_all_displays();

  gpio_reset_pin(CONFIG_GPIO_RESET);
  gpio_set_direction(CONFIG_GPIO_RESET, GPIO_MODE_OUTPUT);
  gpio_set_level(CONFIG_GPIO_RESET, 1);

  gpio_reset_pin(CONFIG_GPIO_DC);
  gpio_set_direction(CONFIG_GPIO_DC, GPIO_MODE_OUTPUT);
  gpio_set_level(CONFIG_GPIO_DC, 0);

  gpio_reset_pin(CONFIG_GPIO_BL);
  gpio_set_direction(CONFIG_GPIO_BL, GPIO_MODE_OUTPUT);
  gpio_set_level(CONFIG_GPIO_BL, 1);

  spi_bus_config_t bus_config = {.sclk_io_num = CONFIG_SCLK_GPIO,
                                 .mosi_io_num = CONFIG_MOSI_GPIO,
                                 .miso_io_num = -1,
                                 .quadwp_io_num = -1,
                                 .quadhd_io_num = -1,
                                 .max_transfer_sz = MAX_TRANSFER_SIZE};

  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

  lcds_reset();

  gpio_set_level(CONFIG_GPIO_BL, 0);
  initialized = true;

  for (size_t i = 0; i < NUM_LCDS; i++) {
    lcd_select(i);
    init_red_tab();
    lcd_blit_rect(0, 0, CONFIG_WIDTH, CONFIG_HEIGHT, framebuffer,
                  sizeof(framebuffer));
  }
}

void deselect_all_displays() {
  for (size_t i = 0; i < NUM_LCDS; i++) {
    gpio_set_level(GPIO_CS_PINS[i], 1);
  }
}

void lcd_select(size_t index) {
  assert(index < NUM_LCDS);
  assert(initialized);

  deselect_all_displays();

  spi_device_interface_config_t tft_devcfg = {
      .clock_speed_hz = SPI_MASTER_FREQ_80M,
      .spics_io_num = GPIO_CS_PINS[index],
      .queue_size = 7,
      .flags = SPI_DEVICE_NO_DUMMY,
  };

  if (spi_device_handle != NULL) {
    ESP_ERROR_CHECK(
        spi_bus_remove_device(spi_device_handle)); // TODO cache up to three
  }

  ESP_ERROR_CHECK(
      spi_bus_add_device(SPI2_HOST, &tft_devcfg, &spi_device_handle));
}

void lcds_reset() {
  gpio_set_level(CONFIG_GPIO_RESET, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(CONFIG_GPIO_RESET, 1);
}

#define CMD_NOP 0x0
#define CMD_SWRESET 0x01
#define CMD_RDDID 0x04
#define CMD_RDDST 0x09

#define CMD_SLPIN 0x10
#define CMD_SLPOUT 0x11
#define CMD_PTLON 0x12
#define CMD_NORON 0x13

#define CMD_INVOFF 0x20
#define CMD_INVON 0x21
#define CMD_DISPOFF 0x28
#define CMD_DISPON 0x29
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_RAMWR 0x2C
#define CMD_RAMRD 0x2E

#define CMD_COLMOD 0x3A
#define CMD_MADCTL 0x36

#define CMD_FRMCTR1 0xB1
#define CMD_FRMCTR2 0xB2
#define CMD_FRMCTR3 0xB3
#define CMD_INVCTR 0xB4
#define CMD_DISSET5 0xB6

#define CMD_PWCTR1 0xC0
#define CMD_PWCTR2 0xC1
#define CMD_PWCTR3 0xC2
#define CMD_PWCTR4 0xC3
#define CMD_PWCTR5 0xC4
#define CMD_VMCTR1 0xC5

#define CMD_RDID1 0xDA
#define CMD_RDID2 0xDB
#define CMD_RDID3 0xDC
#define CMD_RDID4 0xDD

#define CMD_PWCTR6 0xFC

#define CMD_GMCTRP1 0xE0
#define CMD_GMCTRN1 0xE1

void spi_write_bytes(const uint8_t *data, size_t length) {

  if (length <= MAX_TRANSFER_SIZE) {
    spi_transaction_t transaction = {
        .length = length * 8,
        .tx_buffer = data,
    };

    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_device_handle, &transaction));
  } else {
    const uint8_t *remaining_data = data;
    size_t remaining_length = length;

    while (remaining_length > 0) {
      size_t this_send_length = remaining_length;
      if (this_send_length > MAX_TRANSFER_SIZE) {
        this_send_length = MAX_TRANSFER_SIZE;
      }

      spi_transaction_t transaction = {
          .length = this_send_length * 8,
          .tx_buffer = remaining_data,
      };

      ESP_ERROR_CHECK(spi_device_polling_transmit(spi_device_handle, &transaction));

      remaining_length -= this_send_length;
      remaining_data += this_send_length;
    }
  }
}

void write_command_byte(uint8_t command) {
  gpio_set_level(CONFIG_GPIO_DC, 0);
  spi_write_bytes(&command, 1);
}

void write_data_byte(const uint8_t byte) {
  gpio_set_level(CONFIG_GPIO_DC, 1);
  spi_write_bytes(&byte, 1);
}

void write_data_bytes(const uint8_t *data, size_t length) {
  gpio_set_level(CONFIG_GPIO_DC, 1);
  spi_write_bytes(data, length);
}

void init_red_tab() {
  // Init sequence adapted from https://github.com/boochow/MicroPython-ST7735/blob/master/ST7735.py

  write_command_byte(CMD_SWRESET); // reset
  ets_delay_us(150);

  write_command_byte(CMD_SLPOUT); // exit sleep
  ets_delay_us(500);

  {
    uint8_t frtc3[] = {0x01, 0x2C, 0x2D};
    uint8_t frtc6[] = {0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d};

    // set frame rate
    write_command_byte(CMD_FRMCTR1);
    write_data_bytes(frtc3, sizeof(frtc3));

    write_command_byte(CMD_FRMCTR2);
    write_data_bytes(frtc3, sizeof(frtc3));

    write_command_byte(CMD_FRMCTR3);
    write_data_bytes(frtc6, sizeof(frtc6));

    ets_delay_us(10);
  }

  write_command_byte(CMD_INVCTR); // inversion control
  write_data_byte(0x07);          // TODO line inversion?

  // TODO read datasheet, what is this power control stuff
  {
    uint8_t pwr3[] = {0xA2, 0x02, 0x84};
    write_command_byte(CMD_PWCTR1);
    write_data_bytes(pwr3, sizeof(pwr3));
  }

  write_command_byte(CMD_PWCTR2);
  write_data_byte(0xC5); // VGH = 14.7V, VGL = -7.35V

  {
    uint8_t pwr2[] = {
        0x0A, // Opamp current small?,
        0x00  // Boost frequency
    };
    write_command_byte(CMD_PWCTR3);
    write_data_bytes(pwr2, sizeof(pwr2));
  }

  {
    uint8_t pwr2[] = {
        0x8A, // Opamp current small
        0x2A  // Boost frequency
    };
    write_command_byte(CMD_PWCTR4);
    write_data_bytes(pwr2, sizeof(pwr2));
  }

  {
    uint8_t pwr2[] = {
        0x8A, // Opamp current small
        0xEE  // Boost frequency
    };
    write_command_byte(CMD_PWCTR5);
    write_data_bytes(pwr2, sizeof(pwr2));
  }

  write_command_byte(CMD_VMCTR1);
  write_data_byte(0x0E);

  write_command_byte(CMD_INVOFF);

  write_command_byte(CMD_MADCTL);
  write_data_byte(0xC8); // 0xC0 is RGB, 0xC8 is BGR

  write_command_byte(CMD_COLMOD);
  write_data_byte(0x05);

  // TODO gamma curves?
  {
    uint8_t dataGMCTRP[] = {0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22,
                            0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10};
    write_command_byte(CMD_GMCTRP1);
    write_data_bytes(dataGMCTRP, sizeof(dataGMCTRP));

    uint8_t dataGMCTRN[] = {0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2c, 0x29, 0x2e,
                            0x30, 0x30, 0x39, 0x3f, 0x00, 0x07, 0x03, 0x10};
    write_command_byte(CMD_GMCTRN1);
    write_data_bytes(dataGMCTRN, sizeof(dataGMCTRN));
    ets_delay_us(10);
  }

  write_command_byte(CMD_DISPON);
  ets_delay_us(100);

  write_command_byte(CMD_NORON); // normal display on
  ets_delay_us(10);
}

void fill_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
               uint16_t color) {
  assert(x < CONFIG_WIDTH && y < CONFIG_HEIGHT && x + width <= CONFIG_WIDTH &&
         y + height <= CONFIG_HEIGHT);

  for (uint8_t j = 0; j < height; j++) {
    for (uint8_t i = 0; i < width; i++) {
      framebuffer[j * width + i] = color;
    }
  }

  lcd_blit_rect(x, y, width, height, framebuffer, width * height * 2);
}

void lcd_fill(uint16_t color) {
  fill_rect(0, 0, CONFIG_WIDTH, CONFIG_HEIGHT, color);
}

void lcd_copy_rect(int x, int y, int width, int height, const uint16_t *pixels,
                   size_t pixels_size_bytes) {
  assert(pixels_size_bytes < sizeof(framebuffer));
  memcpy(framebuffer, pixels, pixels_size_bytes);
  lcd_blit_rect(x, y, width, height, framebuffer, pixels_size_bytes);
}

void lcd_blit_rect(int x, int y, int width, int height, const uint16_t *pixels,
                   size_t pixels_size_bytes) {
  if (width <= 0 || height <= 0) {
    return;
  }

  spi_device_acquire_bus(spi_device_handle, portMAX_DELAY);

  {
    write_command_byte(CMD_CASET);
    uint16_t xs = CONFIG_OFFSETX + x;
    uint16_t xe = CONFIG_OFFSETX + x + width - 1;
    uint8_t location_data[] = {(xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF,
                               xe & 0xFF};
    write_data_bytes(location_data, sizeof(location_data));
  }

  {
    write_command_byte(CMD_RASET);
    uint16_t ys = CONFIG_OFFSETY + y;
    uint16_t ye = CONFIG_OFFSETY + y + height;
    uint8_t location_data[] = {(ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF,
                               ye & 0xFF};
    write_data_bytes(location_data, sizeof(location_data));
  }

  write_command_byte(CMD_RAMWR);
  assert(pixels_size_bytes == width * height * sizeof(uint16_t));
  write_data_bytes((const uint8_t *)pixels, pixels_size_bytes);

  spi_device_release_bus(spi_device_handle);
}

uint16_t color_to_rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return __builtin_bswap16(((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3));
}