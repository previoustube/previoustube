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

// shared constants
constexpr auto CONFIG_MOSI_GPIO = GPIO_NUM_13;
constexpr auto CONFIG_SCLK_GPIO = GPIO_NUM_12;
constexpr auto CONFIG_GPIO_DC = GPIO_NUM_14;
constexpr auto CONFIG_GPIO_RESET = GPIO_NUM_27;
constexpr auto CONFIG_GPIO_BL = GPIO_NUM_19;

constexpr auto CONFIG_WIDTH = 80;
constexpr auto CONFIG_HEIGHT = 162;
constexpr auto CONFIG_OFFSETX = 24;
constexpr auto CONFIG_OFFSETY = 0;

// unique constants per display
/*
  LCD1 CS pin     33
  LCD2 CS pin     26
  LCD3 CS pin     21
  LCD4 CS pin     0
  LCD5 CS pin     5
  LCD6 CS pin     18
 */

constexpr gpio_num_t GPIO_CS_PINS[NUM_LCDS] = {
    GPIO_NUM_33, GPIO_NUM_26, GPIO_NUM_21, GPIO_NUM_0, GPIO_NUM_5, GPIO_NUM_18};
constexpr auto TAG = "lcds";

static spi_device_handle_t spi_device_handle = nullptr;
static bool initialized = false;
static volatile bool async_tx_in_flight = false;

void init_red_tab();
void deselect_all_displays();

void lcds_init() {
  assert(spi_device_handle == nullptr);
  assert(!initialized);

  ESP_LOGI(TAG, "Configuring LCD SPI devices");

  for (auto pin : GPIO_CS_PINS) {
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
  lcds_off();

  spi_bus_config_t bus_config = {.mosi_io_num = CONFIG_MOSI_GPIO,
                                 .miso_io_num = -1,
                                 .sclk_io_num = CONFIG_SCLK_GPIO,
                                 .quadwp_io_num = -1,
                                 .quadhd_io_num = -1,
                                 .max_transfer_sz = LCD_SPI_MAX_TRANSFER_SIZE};

  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

  lcds_reset();

  lcds_on();
  initialized = true;

  for (size_t i = 0; i < NUM_LCDS; i++) {
    lcd_select(i);
    init_red_tab();
  }
}

void lcds_on() { gpio_set_level(CONFIG_GPIO_BL, 0); }
void lcds_off() { gpio_set_level(CONFIG_GPIO_BL, 1); }

void deselect_all_displays() {
  for (auto i : GPIO_CS_PINS) {
    gpio_set_level(i, 1);
  }
}

void lcd_spi_pre_transfer_cb(spi_transaction_t *t) {
  gpio_set_level(CONFIG_GPIO_DC, (int)t->user);
}

void lcd_select(size_t index) {
  assert(index < NUM_LCDS);
  assert(initialized);
  assert(!async_tx_in_flight);

  deselect_all_displays();

  spi_device_interface_config_t tft_devcfg = {
      .clock_speed_hz = SPI_MASTER_FREQ_40M,
      .spics_io_num = GPIO_CS_PINS[index],
      .flags = SPI_DEVICE_NO_DUMMY,
      .queue_size = 7,
      .pre_cb = lcd_spi_pre_transfer_cb,
  };

  if (spi_device_handle) {
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

void spi_write_bytes(const uint8_t *data, size_t length, uint8_t dc) {
  if (length <= LCD_SPI_MAX_TRANSFER_SIZE) {
    spi_transaction_t transaction = {
        .length = length * 8,
        .user = (void *)(uint32_t)dc,
        .tx_buffer = data,
    };

    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_device_handle, &transaction));
  } else {
    /* TODO all of these should probably be broken into smaller transactions
     * that can fit (e.g. write fewer pixels at a time) */
    const uint8_t *remaining_data = data;
    size_t remaining_length = length;

    while (remaining_length > 0) {
      size_t this_send_length = remaining_length;
      if (this_send_length > LCD_SPI_MAX_TRANSFER_SIZE) {
        this_send_length = LCD_SPI_MAX_TRANSFER_SIZE;
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

inline void write_command_byte(uint8_t command) {
  spi_write_bytes(&command, 1, 0);
}

inline void write_data_byte(const uint8_t byte) {
  spi_write_bytes(&byte, 1, 1);
}

inline void write_data_bytes(const uint8_t *data, size_t length) {
  spi_write_bytes(data, length, 1);
}

void init_red_tab() {
  // Init sequence adapted from
  // https://github.com/boochow/MicroPython-ST7735/blob/master/ST7735.py

  spi_device_acquire_bus(spi_device_handle, portMAX_DELAY);

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

  spi_device_release_bus(spi_device_handle);
}

void lcd_blit_rect(int x, int y, int width, int height, const uint16_t *pixels,
                   size_t pixels_size_bytes) {
  if (width <= 0 || height <= 0) {
    return;
  }

  assert(pixels_size_bytes == width * height * sizeof(uint16_t));

  spi_device_acquire_bus(spi_device_handle, portMAX_DELAY);

  {
    write_command_byte(CMD_CASET);
    uint16_t xs = CONFIG_OFFSETX + x;
    uint16_t xe = CONFIG_OFFSETX + x + width - 1;
    uint8_t location_data[] = {static_cast<uint8_t>((xs >> 8) & 0xFF),
                               static_cast<uint8_t>(xs & 0xFF),
                               static_cast<uint8_t>((xe >> 8) & 0xFF),
                               static_cast<uint8_t>(xe & 0xFF)};
    write_data_bytes(location_data, sizeof(location_data));
  }

  {
    write_command_byte(CMD_RASET);
    uint16_t ys = CONFIG_OFFSETY + y;
    uint16_t ye = CONFIG_OFFSETY + y + height;
    uint8_t location_data[] = {static_cast<uint8_t>((ys >> 8) & 0xFF),
                               static_cast<uint8_t>(ys & 0xFF),
                               static_cast<uint8_t>((ye >> 8) & 0xFF),
                               static_cast<uint8_t>(ye & 0xFF)};
    write_data_bytes(location_data, sizeof(location_data));
  }

  write_command_byte(CMD_RAMWR);
  write_data_bytes((const uint8_t *)pixels, pixels_size_bytes);

  spi_device_release_bus(spi_device_handle);
}

uint16_t color_to_rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return __builtin_bswap16(((red & 0xF8) << 8) | ((green & 0xFC) << 3) |
                           (blue >> 3));
}