//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include "esp_attr.h"
#include "esp_log.h"

#include "png.h"
extern "C" {
#include "lcds.h"
}

#include "PNGdec.h"
#include <cstdio>

static const char *TAG = "png";

static PNG *png;

static uint32_t background_color;

#define ROW_PIXELS 80

DMA_ATTR uint16_t
    rowOfPixels[ROW_PIXELS]; // FIXME not robust to other image sizes at all

void png_draw_callback(PNGDRAW *pdraw) {
  assert(png);
  assert(pdraw->iWidth == ROW_PIXELS);
  png->getLineAsRGB565(pdraw, rowOfPixels, PNG_RGB565_BIG_ENDIAN,
                       background_color);

  lcd_blit_rect(0, pdraw->y, pdraw->iWidth, 1, rowOfPixels,
                sizeof(rowOfPixels));
}

int32_t png_read_callback(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  FILE *f = (FILE *)pFile->fHandle;
  if (!f)
    return 0;
  return (int32_t)fread(pBuf, 1, iLen, f);
}

int32_t png_seek_callback(PNGFILE *pFile, int32_t iPosition) {
  FILE *f = (FILE *)pFile->fHandle;
  if (!f)
    return 0;
  return fseek(f, iPosition, SEEK_SET);
}

void *png_open_callback(const char *szFilename, int32_t *pFileSize) {
  FILE *f = fopen(szFilename, "rb");
  fseek(f, 0, SEEK_END);
  *pFileSize = ftell(f);
  fseek(f, 0, SEEK_SET);
  return f;
}

void png_close_callback(void *pHandle) {
  FILE *f = (FILE *)pHandle;
  fclose(f);
}

void png_open(const char *filename) {
  ESP_LOGI(TAG, "Opening image file %s", filename);
  assert(png == nullptr);
  png = new PNG();

  int ret = png->open(filename, png_open_callback, png_close_callback,
                      png_read_callback, png_seek_callback, png_draw_callback);

  assert(ret == 0);
  png->decode(nullptr, 0);
}

void png_draw(uint32_t background) {
  assert(png);

  background_color = background;
  png->decode(nullptr, 0);
}

void png_decode_callback(PNGDRAW *pdraw) {
  assert(png);

  auto *buffer = static_cast<uint16_t *>(pdraw->pUser);

  assert(pdraw->iWidth == ROW_PIXELS);
  png->getLineAsRGB565(pdraw, buffer + (pdraw->y * pdraw->iWidth),
                       PNG_RGB565_BIG_ENDIAN, -1);
}

void png_decode(const char *filename, uint16_t *buffer, size_t size) {
  assert(png == nullptr);
  png = new PNG();
  int ret =
      png->open(filename, png_open_callback, png_close_callback,
                png_read_callback, png_seek_callback, png_decode_callback);
  assert(ret == 0);
  assert(size >= png->getWidth() * png->getHeight() * 2);

  png->decode(buffer, 0);
  png->close();
  free(png);
  png = nullptr;
}

void png_close() {
  assert(png);
  png->close();
  free(png);
  png = nullptr;
}
