//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#pragma once

#include "lvgl.h"

using flapper_finished_callback = void (*)(void *user_data);

class flapper {
public:
  explicit flapper(lv_obj_t *screen) {
    this->screen = screen;
  }

  void before();
  void after();

  void start(bool last);
  void stop();

  ~flapper();

  void set_finished_callback(flapper_finished_callback callback, void *user_data) {
    this->finished_callback = callback;
    this->finished_callback_user_data = user_data;
  }

private:
  void animate(int32_t value);
  void animation_deleted();
  void draw_overlay(lv_event_t *event);
  void cancel_existing_animation();

  lv_obj_t *screen;

  lv_img_dsc_t snapshot1{};
  void *snapshot1_buffer{};
  lv_img_dsc_t snapshot2{};
  void *snapshot2_buffer{};

  lv_obj_t *overlay{};
  lv_coord_t divider_y{};
  lv_coord_t last_divider_y{};
  flapper_finished_callback finished_callback{};
  void *finished_callback_user_data{};
  lv_coord_t compute_center_axis_y() const;

  // TODO remove
  void copy_partial_image_perspective(
      uint8_t *destination_buffer, lv_coord_t buf_width, lv_coord_t buf_height,
      const lv_area_t &dest_area, const lv_area_t &unclipped_dest_area,
      const lv_img_dsc_t &source_image, const lv_area_t &source_area,
      bool invert);
};
