//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#include <SDL2/SDL.h>
#include <cassert>
#include <stdexcept>
#include <unistd.h>

#include "lvgl.h"

#include "clock.h"
#include "drivers/lcds.h"
#include "gui.h"
#include "spiram_allocate.h"

constexpr auto MARGIN_SIZE = 30;
constexpr int WINDOW_WIDTH = LCD_WIDTH * 6 + MARGIN_SIZE * 5;
constexpr int WINDOW_HEIGHT = LCD_HEIGHT;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

constexpr auto BUFFER_ROWS =
    LCD_SPI_MAX_TRANSFER_SIZE / LCD_WIDTH / sizeof(uint16_t);
constexpr auto PIXEL_BUFFER_SIZE_PX = LCD_WIDTH * BUFFER_ROWS;

static uint16_t lcd_buffers[NUM_LCDS][PIXEL_BUFFER_SIZE_PX];
static lv_disp_draw_buf_t draw_buffers[NUM_LCDS];
static lv_disp_drv_t display_drivers[NUM_LCDS];
static lv_disp_t *displays[NUM_LCDS];

struct driver_user_data {
  size_t display_index;
};
static struct driver_user_data driver_user_datas[NUM_LCDS];

static void flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                     lv_color_t *color_p) {
  auto *user_data = static_cast<driver_user_data *>(disp_drv->user_data);

  SDL_Rect update_area = {
      .x = static_cast<int>(area->x1 + user_data->display_index *
                                           (LCD_WIDTH + MARGIN_SIZE)),
      .y = area->y1,
      .w = area->x2 - area->x1 + 1,
      .h = area->y2 - area->y1 + 1};
  SDL_UpdateTexture(texture, &update_area, color_p,
                    (int)update_area.w * sizeof(uint16_t));
  SDL_RenderCopy(renderer, texture, &update_area, &update_area);
  SDL_RenderPresent(renderer);

  lv_disp_flush_ready(disp_drv);
}

static void sdl_init() {
  SDL_Init(SDL_INIT_VIDEO);

  window =
      SDL_CreateWindow("PreviousTube Simulator", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                        SDL_TEXTUREACCESS_STATIC, WINDOW_WIDTH, WINDOW_HEIGHT);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

  for (size_t i = 0; i < NUM_LCDS; i++) {
    lv_disp_draw_buf_init(&draw_buffers[i], lcd_buffers[i], nullptr,
                          PIXEL_BUFFER_SIZE_PX);
    lv_disp_drv_t *driver = &display_drivers[i];
    lv_disp_drv_init(driver);
    driver->draw_buf = &draw_buffers[i];
    driver->hor_res = LCD_WIDTH;
    driver->ver_res = LCD_HEIGHT;
    driver->flush_cb = flush_cb;

    struct driver_user_data *user_data = &driver_user_datas[i];
    user_data->display_index = i;

    driver->user_data = user_data;

    displays[i] = lv_disp_drv_register(driver);
  }
}

void cleanup() {
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();
}

lv_disp_t *gui_get_display(size_t index) {
  assert(lv_is_initialized());
  assert(index < NUM_LCDS);
  return displays[index];
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  lv_init();

  sdl_init();

  clock::get().update();

  while (true) {
    lv_timer_handler();
    usleep(5 * 1000);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EventType::SDL_QUIT:
        cleanup();
        return 0;
      case SDL_EventType::SDL_MOUSEBUTTONDOWN:
        clock::get().shuffle();
        break;
      }
    }
  }
}

extern "C" void my_assert(char *file, int line) {
  throw std::runtime_error("Assertion failed: " + std::string(file) + ":" +
                           std::to_string(line));
}

void *spiram_allocate(size_t size) { return malloc(size); }

void spiram_free(void *ptr) {
  if (ptr == nullptr) {
    return;
  }
  free(ptr);
}
