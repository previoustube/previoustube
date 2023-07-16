//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include "gui.h"
#include "esp_timer.h"
#include "lcds.h"
#include <esp_freertos_hooks.h>
#include <esp_log.h>
#include <lvgl.h>

#include "esp_attr.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(GUI_EVENTS);

enum {
  GUI_EVENT_TIMER,
};

static uint16_t *lcd_buffers[NUM_LCDS];
static lv_disp_draw_buf_t draw_buffers[NUM_LCDS];
static lv_disp_drv_t display_drivers[NUM_LCDS];
static lv_disp_t *displays[NUM_LCDS];

struct driver_user_data {
  size_t display_index;
};

static struct driver_user_data driver_user_datas[NUM_LCDS];
static esp_timer_handle_t lv_timer_handle;

static void flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                     lv_color_t *color_p) {
  struct driver_user_data *user_data = disp_drv->user_data;
  lcd_select(user_data->display_index);
  lcd_copy_rect(area->x1, area->y1, area->x2 - area->x1 + 1,
                area->y2 - area->y1 + 1, (const uint16_t *)color_p,
                (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) *
                    sizeof(lv_color_t));
  lv_disp_flush_ready(disp_drv);
}

static void timer_callback(void *arg) {
  esp_err_t err = esp_event_post(GUI_EVENTS, GUI_EVENT_TIMER, NULL, 0, 10);
  if (err != ESP_ERR_TIMEOUT) { // this is ok if we're busy
    ESP_ERROR_CHECK(err);
  }
}

static void timer_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t id, void *event_data) {
  lv_timer_handler();
}

static void log_cb(const char *buf) { ESP_LOGI("lvgl", "%s", buf); }

void gui_init() {
  lv_log_register_print_cb(log_cb);

  lv_init();

  for (size_t i = 0; i < NUM_LCDS; i++) {
    lcd_buffers[i] = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
                                      MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buffers[i], lcd_buffers[i], NULL,
                          LCD_WIDTH * LCD_HEIGHT);
    lv_disp_drv_t *driver = &display_drivers[i];
    lv_disp_drv_init(driver);
    driver->direct_mode = true;
    driver->draw_buf = &draw_buffers[i];
    driver->hor_res = LCD_WIDTH;
    driver->ver_res = LCD_HEIGHT;
    driver->flush_cb = flush_cb;

    struct driver_user_data *user_data = &driver_user_datas[i];
    user_data->display_index = i;

    driver->user_data = user_data;

    displays[i] = lv_disp_drv_register(driver);
  }

  esp_timer_create_args_t timer_args = {
      .callback = timer_callback,
      .name = "lv_timer",
      .dispatch_method = ESP_TIMER_TASK,
      .skip_unhandled_events = true,
  };

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      GUI_EVENTS, GUI_EVENT_TIMER, timer_event_handler, NULL, NULL));

  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &lv_timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lv_timer_handle, 5000));

  lv_img_cache_set_size(8);
}

lv_disp_t *gui_get_display(size_t index) {
  assert(lv_is_initialized());
  assert(index < NUM_LCDS);
  return displays[index];
}

ESP_EVENT_DEFINE_BASE(GUI_EVENTS);
