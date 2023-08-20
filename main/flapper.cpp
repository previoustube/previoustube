//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#include "flapper.h"
#include "fpm/fixed.hpp"
#include "fpm/math.hpp"
#include "spiram_allocate.h"
#include <algorithm>
#include <cassert>

// TODO remove
#include "gui.h"
#include <cstdio>

// constexpr auto FLIP_DURATION_MS = 5000;
constexpr auto FLIP_DURATION_MS = 1000;

void flapper::before() {
  cancel_existing_animation();

  size_t buffer_size =
      lv_snapshot_buf_size_needed(screen, LV_IMG_CF_TRUE_COLOR);

  spiram_free(snapshot1_buffer);
  snapshot1 = {};
  snapshot1_buffer = spiram_allocate(buffer_size);
  lv_res_t result = lv_snapshot_take_to_buf(
      screen, LV_IMG_CF_TRUE_COLOR, &snapshot1, snapshot1_buffer, buffer_size);
  assert(result == LV_RES_OK);
}

void flapper::after() {
  cancel_existing_animation();

  size_t buffer_size =
      lv_snapshot_buf_size_needed(screen, LV_IMG_CF_TRUE_COLOR);

  spiram_free(snapshot2_buffer);
  snapshot2 = {};
  snapshot2_buffer = spiram_allocate(buffer_size);
  lv_res_t result = lv_snapshot_take_to_buf(
      screen, LV_IMG_CF_TRUE_COLOR, &snapshot2, snapshot2_buffer, buffer_size);
  assert(result == LV_RES_OK);
}

void flapper::cancel_existing_animation() {
  if (overlay != nullptr) {
    lv_obj_del(overlay);
    overlay = nullptr;
  }
  lv_anim_del(screen, nullptr);
}

void flapper::start(bool last) {
  assert(snapshot1_buffer != nullptr);
  assert(snapshot2_buffer != nullptr);
  lv_anim_del(screen, nullptr);

  lv_anim_t animation;
  lv_anim_init(&animation);

  lv_anim_set_user_data(&animation, this);
  lv_anim_set_var(&animation, screen); // only really used for deduplication
  lv_anim_set_custom_exec_cb(&animation, [](lv_anim_t *anim, int32_t value) {
    auto *instance = static_cast<flapper *>(anim->user_data);
    instance->animate(value);
  });
  lv_anim_set_deleted_cb(&animation, [](lv_anim_t *anim) {
    auto *instance = static_cast<flapper *>(anim->user_data);
    instance->animation_deleted();
  });

  lv_anim_set_time(&animation, FLIP_DURATION_MS);
  lv_anim_set_path_cb(&animation,
                      last ? lv_anim_path_bounce : lv_anim_path_ease_in_out);
  lv_anim_set_values(&animation, 0, lv_obj_get_height(screen));

  lv_coord_t width = lv_obj_get_width(screen);
  lv_coord_t height = lv_obj_get_height(screen);

  if (overlay != nullptr) {
    lv_obj_del(overlay);
  }
  overlay = lv_obj_create(screen);
  lv_obj_remove_style_all(overlay);
  //  lv_obj_set_style_opa(overlay, LV_OPA_COVER, LV_PART_ANY);
  lv_obj_add_event_cb(
      overlay,
      [](lv_event_t *event) {
        auto *instance = static_cast<flapper *>(lv_event_get_user_data(event));
        instance->draw_overlay(event);
      },
      LV_EVENT_DRAW_MAIN, this);
  lv_obj_set_size(overlay, width, height);

  lv_anim_start(&animation);
}

flapper::~flapper() {
  lv_anim_del(screen, nullptr);

  if (snapshot1_buffer != nullptr) {
    spiram_free(snapshot1_buffer);
    snapshot1_buffer = nullptr;
  }
  if (snapshot2_buffer != nullptr) {
    spiram_free(snapshot2_buffer);
    snapshot2_buffer = nullptr;
  }

  if (overlay != nullptr) {
    lv_obj_del(overlay);
  }
}

void flapper::animate(int32_t value) {
  if (overlay == nullptr) {
    return;
  }

  lv_coord_t axis = compute_center_axis_y();
  lv_area_t dirty_area = {.x1 = 0,
                          .y1 = std::min(last_divider_y, axis),
                          .x2 = lv_obj_get_width(overlay),
                          .y2 = std::max(axis, (lv_coord_t)(value + 3))};
  lv_obj_invalidate_area(overlay, &dirty_area);

  last_divider_y = divider_y;
  divider_y = static_cast<lv_coord_t>(value);
}

void flapper::animation_deleted() {
  if (finished_callback) {
    lv_async_call(
        [](void *user_data) {
          auto *instance = static_cast<flapper *>(user_data);
          instance->finished_callback(instance->finished_callback_user_data);
        },
        this);
  }

  if (snapshot1_buffer != nullptr) {
    spiram_free(snapshot1_buffer);
    snapshot1_buffer = nullptr;
  }
  snapshot1 = {};

  if (snapshot2_buffer != nullptr) {
    spiram_free(snapshot2_buffer);
    snapshot2_buffer = nullptr;
  }
  snapshot2 = {};

  if (overlay != nullptr) {
    lv_obj_del(overlay);
    overlay = nullptr;
  }
}

void copy_partial_image_flat(uint8_t *destination_buffer, lv_coord_t buf_width,
                             lv_coord_t buf_height, const lv_area_t &dest_area,
                             const lv_img_dsc_t &source_image,
                             const lv_area_t &source_area) {
  LV_ASSERT(lv_area_get_width(&source_area) == lv_area_get_width(&dest_area));
  LV_ASSERT(lv_area_get_height(&source_area) == lv_area_get_height(&dest_area));

  const size_t bytes_per_buf_row = buf_width * sizeof(lv_color_t);
  const uint32_t source_image_width = source_image.header.w;
  const uint32_t source_image_height = source_image.header.h;
  const size_t bytes_per_src_row = source_image_width * sizeof(lv_color_t);
  const size_t width_pixels = source_area.x2 - source_area.x1 + 1;

  for (lv_coord_t dst_y = dest_area.y1, src_y = source_area.y1;
       dst_y <= dest_area.y2 && src_y <= source_area.y2; dst_y++, src_y++) {

    const size_t dst_offset =
        bytes_per_buf_row * dst_y + dest_area.x1 * sizeof(lv_color_t);
    uint8_t *dst_ptr = destination_buffer + dst_offset;

    const size_t src_offset =
        bytes_per_src_row * src_y + source_area.x1 * sizeof(lv_color_t);
    const uint8_t *src_ptr = source_image.data + src_offset;

    size_t copy_size = width_pixels * sizeof(lv_color_t);

    LV_ASSERT(src_offset + copy_size <=
              source_image_width * source_image_height * sizeof(lv_color_t));
    LV_ASSERT(dst_offset + copy_size <=
              buf_width * buf_height * sizeof(lv_color_t));
    lv_memcpy(dst_ptr, src_ptr, copy_size);
  }
}

void flapper::copy_partial_image_perspective(
    uint8_t *destination_buffer, lv_coord_t buf_width, lv_coord_t buf_height,
    const lv_area_t &dest_area, const lv_area_t &unclipped_dest_area,
    const lv_img_dsc_t &source_image, const lv_area_t &source_area,
    bool invert) {
  LV_ASSERT(lv_area_get_width(&source_area) == lv_area_get_width(&dest_area));

  const size_t bytes_per_buf_row = buf_width * sizeof(lv_color_t);
  const uint32_t source_image_width = source_image.header.w;
  const uint32_t source_image_height = source_image.header.h;
  const size_t bytes_per_src_row = source_image_width * sizeof(lv_color_t);

  const size_t width_pixels = source_area.x2 - source_area.x1 + 1;

  // Interpolating src_y is how we get the 3D effect. We can tell from the
  // height difference of the source area and the destination area where
  // in the animation we are, and solve for what the depth is at that point
  // using the equation of a circle, supplying Y and R and solving for X. That
  // is the maximum depth of the imaginary 3D rectangle we are drawing. Then
  // we can use the standard perspective-correct texture interpolation formula
  // to figure out which row of the source image to use for each row of the
  // area being drawn. Since we are always looking forward onto the screen
  // surface, we only have to do this once per row rather than for every
  // pixel, keeping the calculation (relatively) cheap CPU-wise

  int32_t radius = source_area.y2 - source_area.y1;

  lv_coord_t unclipped_dest_height = lv_area_get_height(&unclipped_dest_area);
  int32_t y_pos = lv_area_get_height(&source_area) - unclipped_dest_height;

  fpm::fixed_16_16 depth =
      1 - (fpm::sqrt(fpm::fixed_16_16(radius * radius - y_pos * y_pos)) / fpm::fixed_16_16(radius));
  //  lv_sqrt_res_t depth_fp;
  //  lv_sqrt(radius * radius - y_pos * y_pos, &depth_fp, 0x800);

  fpm::fixed_16_16 max_depth =
      fpm::fixed_16_16(1) + (invert ? fpm::fixed_16_16(0) : depth);
  fpm::fixed_16_16 min_depth =
      fpm::fixed_16_16(1) + (invert ? depth : fpm::fixed_16_16(0));

  for (lv_coord_t dst_y = dest_area.y1; dst_y <= dest_area.y2; dst_y++) {
    fpm::fixed_16_16 interpolation_alpha =
        fpm::fixed_16_16(dst_y - unclipped_dest_area.y1) /
        fpm::fixed_16_16(unclipped_dest_height);

    // U_alpha = ((1 - alpha) * (U_0 / Z_0) + alpha * (U_1 / Z_1)) / ((1 -
    // alpha) * (1 / Z_0) + alpha * (1 / Z_1))
    fpm::fixed_16_16 src_yf =
        (((fpm::fixed_16_16(1) - interpolation_alpha) *
              fpm::fixed_16_16(source_area.y1) / min_depth +
          interpolation_alpha * fpm::fixed_16_16(source_area.y2) / max_depth) /
         ((fpm::fixed_16_16(1) - interpolation_alpha) / min_depth +
          interpolation_alpha / max_depth));

    //    auto linear_src_yf = (1.0f - interpolation_alpha) *
    //    (float)source_area.y1 +
    //                         interpolation_alpha * (float)source_area.y2;

    //    if(lv_obj_get_screen(overlay) ==
    //    lv_disp_get_scr_act(gui_get_display(0))) {
    //      printf("y_pos: %d, depth: %f, linear: %f, persp: %f\n", y_pos,
    //      depth, linear_src_yf, src_yf);
    //    }

    // clamp for safety
    lv_coord_t src_y =
        std::max(std::min((lv_coord_t)src_yf, (lv_coord_t)source_area.y2),
                 (lv_coord_t)source_area.y1);

    const size_t dst_offset =
        bytes_per_buf_row * dst_y + dest_area.x1 * sizeof(lv_color_t);
    uint8_t *dst_ptr = destination_buffer + dst_offset;

    const size_t src_offset =
        bytes_per_src_row * src_y + source_area.x1 * sizeof(lv_color_t);
    const uint8_t *src_ptr = source_image.data + src_offset;

    size_t copy_size = width_pixels * sizeof(lv_color_t);

    LV_ASSERT(src_offset + copy_size <=
              source_image_width * source_image_height * sizeof(lv_color_t));
    LV_ASSERT(dst_offset + copy_size <=
              buf_width * buf_height * sizeof(lv_color_t));
    lv_memcpy(dst_ptr, src_ptr, copy_size);
  }
}

void flapper::draw_overlay(lv_event_t *event) {
  auto *draw_ctx = lv_event_get_draw_ctx(event);
  auto *destination_buffer = static_cast<uint8_t *>(draw_ctx->buf);

  // preconditions
  LV_ASSERT((lv_img_cf_get_px_size(snapshot1.header.cf) >> 3) ==
                sizeof(lv_color_t) &&
            snapshot1.header.cf == LV_IMG_CF_TRUE_COLOR);
  LV_ASSERT((lv_img_cf_get_px_size(snapshot2.header.cf) >> 3) ==
            sizeof(lv_color_t));
  lv_coord_t buf_width = lv_area_get_width(draw_ctx->buf_area);
  lv_coord_t buf_height = lv_area_get_height(draw_ctx->buf_area);

  lv_coord_t axis = compute_center_axis_y();

  // top part
  {
    lv_area_t obj_area, source_area, dest_area;
    lv_obj_get_coords(overlay, &obj_area);
    lv_coord_t left = obj_area.x1;
    lv_coord_t top = obj_area.y1;
    obj_area.y2 =
        (lv_coord_t)(obj_area.y1 +
                     std::min((lv_coord_t)(divider_y - 1),
                              axis)); // stop at the divider or midpoint
    if (_lv_area_intersect(&source_area, draw_ctx->clip_area, &obj_area)) {
      lv_area_copy(&dest_area,
                   &source_area); // this is currently in absolute coordinates

      // move to the upper left corner of the image itself rather than the obj
      // location for the source
      lv_area_move(&source_area, (lv_coord_t)(-left), (lv_coord_t)(-top));

      // relativize to the upper left corner of the buffer area itself for the
      // destination
      lv_area_move(&dest_area, (lv_coord_t)(-draw_ctx->buf_area->x1),
                   (lv_coord_t)(-draw_ctx->buf_area->y1));

      copy_partial_image_flat(destination_buffer, buf_width, buf_height,
                              dest_area, snapshot2, source_area);
    }
  }

  if (divider_y < axis) {
    lv_area_t obj_area, dest_area;
    lv_obj_get_coords(overlay, &obj_area);
    lv_coord_t left = obj_area.x1;
    lv_coord_t top = obj_area.y1;
    obj_area.y1 = top + divider_y;
    obj_area.y2 = top + axis - 3;

    lv_area_t source_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = static_cast<lv_coord_t>(snapshot1.header.w - 1),
        .y2 = static_cast<lv_coord_t>(axis - 1)};

    if (_lv_area_intersect(&dest_area, draw_ctx->clip_area, &obj_area)) {
      // relativize to the upper left corner of the buffer area itself for the
      // destination
      lv_area_move(&dest_area, (lv_coord_t)(-draw_ctx->buf_area->x1),
                   (lv_coord_t)(-draw_ctx->buf_area->y1));
      lv_area_move(&obj_area, (lv_coord_t)(-draw_ctx->buf_area->x1),
                   (lv_coord_t)(-draw_ctx->buf_area->y1));

      copy_partial_image_perspective(destination_buffer, buf_width, buf_height,
                                     dest_area, obj_area, snapshot1,
                                     source_area, false);
    }
  }

  // divider line
  {
    lv_area_t divider_area;
    lv_obj_get_coords(overlay, &divider_area);
    lv_area_move(&divider_area, 0, (lv_coord_t)(divider_y));
    lv_area_set_height(&divider_area, 1);

    lv_draw_rect_dsc_t divider_dsc;
    lv_draw_rect_dsc_init(&divider_dsc);
    divider_dsc.bg_color = lv_color_hex3(0x999);
    lv_draw_rect(draw_ctx, &divider_dsc, &divider_area);

    lv_area_move(&divider_area, 0, 1);
    divider_dsc.bg_color = lv_color_hex3(0x333);
    lv_draw_rect(draw_ctx, &divider_dsc, &divider_area);

    lv_area_move(&divider_area, 0, 1);
    divider_dsc.bg_color = lv_color_hex3(0x555);
    lv_draw_rect(draw_ctx, &divider_dsc, &divider_area);
  }

  if (divider_y > axis) {
    lv_area_t obj_area, dest_area;
    lv_obj_get_coords(overlay, &obj_area);
    lv_coord_t top = obj_area.y1;
    obj_area.y1 = top + axis;
    obj_area.y2 = top + divider_y;

    lv_area_t source_area = {
        .x1 = 0,
        .y1 = axis,
        .x2 = static_cast<lv_coord_t>(snapshot1.header.w - 1),
        .y2 = static_cast<lv_coord_t>(snapshot1.header.h - 1),
    };

    if (_lv_area_intersect(&dest_area, draw_ctx->clip_area, &obj_area)) {
      // relativize to the upper left corner of the buffer area itself for the
      // destination
      lv_area_move(&dest_area, (lv_coord_t)(-draw_ctx->buf_area->x1),
                   (lv_coord_t)(-draw_ctx->buf_area->y1));
      lv_area_move(&obj_area, (lv_coord_t)(-draw_ctx->buf_area->x1),
                   (lv_coord_t)(-draw_ctx->buf_area->y1));

      copy_partial_image_perspective(destination_buffer, buf_width, buf_height,
                                     dest_area, obj_area, snapshot2,
                                     source_area, false);
    }
  }

  // bottom part
  {
    lv_area_t obj_area, source_area, dest_area;
    lv_obj_get_coords(overlay, &obj_area);
    lv_coord_t left = obj_area.x1;
    lv_coord_t top = obj_area.y1;
    obj_area.y1 =
        (lv_coord_t)(obj_area.y1 + std::max(axis, (lv_coord_t)(divider_y + 3)));
    if (_lv_area_intersect(&source_area, draw_ctx->clip_area, &obj_area)) {
      lv_area_copy(&dest_area,
                   &source_area); // this is currently in absolute coordinates

      // move to the upper left corner of the image itself rather than the obj
      // location for the source
      lv_area_move(&source_area, (lv_coord_t)(-left), (lv_coord_t)(-top));

      // relativize to the upper left corner of the buffer area itself for the
      // destination
      lv_area_move(&dest_area, (lv_coord_t)(-draw_ctx->buf_area->x1),
                   (lv_coord_t)(-draw_ctx->buf_area->y1));

      copy_partial_image_flat(destination_buffer, buf_width, buf_height,
                              dest_area, snapshot1, source_area);
    }
  }
}

lv_coord_t flapper::compute_center_axis_y() const {
  lv_area_t content_area;
  lv_obj_get_coords(overlay, &content_area);
  auto axis = (lv_coord_t)(lv_area_get_height(&content_area) / 2 - 4);
  return axis;
}

void flapper::stop() { lv_anim_del(screen, nullptr); }
