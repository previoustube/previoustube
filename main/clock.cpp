//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#include "clock.h"
#include "drivers/lcds.h"
#include "gui.h"
#include <ctime>

#include <lvgl.h>

constexpr auto FLIP_SPACING_MS = 700;

const std::vector<std::string> digits_loop = {"",  ":", "0", "1", "2", "3",
                                              "4", "5", "6", "7", "8", "9"};

const std::vector<std::string> divider_loop = {"", ":"};

static const lv_color_t TEXT_COLOR = lv_color_hex(0xFCF9D9);

static void timer_callback(lv_timer_t *timer) {
  auto *instance = static_cast<class clock *>(timer->user_data);
  instance->update();
}

clock::clock() {
  for (int i = 0; i < NUM_LCDS; i++) {
    lv_disp_set_default(gui_get_display(i));
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *background_image = lv_img_create(screen);
    background_images[i] = background_image;
    lv_img_set_src(background_image, "S:/spiffs/split_flap.png");
    lv_obj_set_pos(background_image, 0, 0);

    flappers[i] = new flapper(background_image);

    lv_obj_set_style_text_color(screen, TEXT_COLOR, LV_PART_MAIN);

    if (i != NUM_LCDS - 1) {
      lv_obj_set_style_text_font(screen, &oswald_100, LV_PART_MAIN);

      lv_obj_t *label = lv_label_create(background_image);
      lv_label_set_text_static(label, "");
      digit_labels[i] = label;

      lv_obj_align(label, LV_ALIGN_CENTER, 0, -7);
    } else {
      lv_obj_set_style_text_font(screen, &oswald_60, LV_PART_MAIN);

      ampm_label_top = lv_label_create(background_image);
      lv_obj_set_style_text_align(ampm_label_top, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_align(ampm_label_top, LV_ALIGN_CENTER, 0, -44);
      lv_label_set_text_static(ampm_label_top, "");

      ampm_label_bottom = lv_label_create(background_image);
      lv_obj_set_style_text_align(ampm_label_bottom, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_align(ampm_label_bottom, LV_ALIGN_CENTER, 0, 33);
      lv_label_set_text_static(ampm_label_bottom, "");
    }

    lv_obj_t *divider_image = lv_img_create(background_image);
    lv_obj_set_pos(divider_image, 8, 75);
    lv_img_set_src(divider_image, "S:/spiffs/split_flap_divider.png");
  }

  clock_update_timer = lv_timer_create(timer_callback, 60000, this);
}

void clock::update() {
  time_t now = 0;
  char strftime_buf[64];
  struct tm timeinfo {};

  time(&now);

#ifdef __MINGW32__
  localtime_s(&timeinfo, &now);
#else
  localtime_r(&now, &timeinfo);
#endif

  strftime(strftime_buf, sizeof(strftime_buf), "%I:%M%p", &timeinfo);

  char *next_digit = strftime_buf;
  size_t i = 0;
  uint32_t delay = 0;

  for (auto &digit_label : digit_labels) {
    char digit = *next_digit;
    char text[2] = {digit, '\0'};

    if (digit != '\0') {
      next_digit++;
    }

    char *existing_text = lv_label_get_text(digit_label);

    if (strcmp(existing_text, text) != 0) {
      flapper *flapper = flappers[i];
      std::vector<std::string> values = {};

      auto existing_string = std::string(existing_text);
      auto desired_string = std::string(text);
      auto &character_loop = (desired_string == ":") ? divider_loop : digits_loop;

      auto start_iter =
          std::find(character_loop.cbegin(),
                                  character_loop.cend(), existing_string);
      auto end_iter =
          std::find(character_loop.cbegin(), character_loop.cend(), desired_string);

      LV_ASSERT(start_iter != character_loop.cend());
      LV_ASSERT(end_iter != character_loop.cend());

      auto iter = start_iter;
      while (true) {
        iter++;
        if (iter == character_loop.cend()) {
          iter = character_loop.cbegin();
        }

        auto &value = *iter;
        values.push_back(value);
        if (iter == end_iter) {
          break;
        }
      }

      auto existing_timer = delayed_start_timers[i];
      if (existing_timer != nullptr) {
        lv_timer_del(existing_timer);
        delayed_start_timers[i] = nullptr;
      }

      flap_sequences[i] = std::make_unique<flap_sequence>(
          flapper,
          [&digit_label](const std::string &value) {
            lv_label_set_text(digit_label, value.c_str());
          },
          values);

      struct timer_user_data {
        clock *this_;
        size_t index;
      };
      auto *start_delayed_timer = lv_timer_create(
          [](lv_timer_t *timer) {
            auto *user_data =
                static_cast<struct timer_user_data *>(timer->user_data);
            user_data->this_->delayed_start_flap_sequence(user_data->index);

            delete user_data;
          },
          delay, new timer_user_data{this, i});
      lv_timer_set_repeat_count(start_delayed_timer, 1);

      delay += FLIP_SPACING_MS;
    }

    i++;
  }

  char digit = *next_digit;

  char *existing_ampm_text = lv_label_get_text(ampm_label_top);
  char existing_digit = *existing_ampm_text;

  if (digit != existing_digit) {
    flapper *flapper = flappers[i];
    flapper->before();
    if (digit == 'A') {
      lv_label_set_text_static(ampm_label_top, "A");
    } else if (digit == 'P') {
      lv_label_set_text_static(ampm_label_top, "P");
    }

    lv_label_set_text_static(ampm_label_bottom, "M");
    flapper->after();
    flapper->start(true);
  }

  auto remaining_seconds = 60 - timeinfo.tm_sec;
  lv_timer_set_period(clock_update_timer, remaining_seconds * 1000);
  lv_timer_reset(clock_update_timer);
}

void clock::delayed_start_flap_sequence(size_t index) {
  flap_sequences[index]->start();
  delayed_start_timers[index] = nullptr;
}

void clock::shuffle() {
  char buf[10] = {
      0,
  };
  snprintf(buf, sizeof(buf), "%d", rand() % 10);
  buf[sizeof(buf) - 1] = '\0';

  for (auto &digit_label : digit_labels) {
    if(digit_label == digit_labels[2]) {
      lv_label_set_text_static(digit_label, "");
    } else {
      lv_label_set_text_static(digit_label, buf);
    }
  }

  lv_label_set_text_static(ampm_label_top, "");
  lv_label_set_text_static(ampm_label_bottom, "");

  update();
}

clock::~clock() {
  lv_timer_del(clock_update_timer);

  for (auto &flapper : flappers) {
    delete flapper;
  }

  lv_obj_del(ampm_label_top);
  lv_obj_del(ampm_label_bottom);

  for (auto &digit_label : digit_labels) {
    lv_obj_del(digit_label);
  }

  for (auto &image : background_images) {
    lv_obj_del(image);
  }
}
