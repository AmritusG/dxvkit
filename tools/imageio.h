/*
 * dxvtool image input: PNG (8 / 16-bit, non-interlaced, via the system zlib), PAM / PPM, raw RGBA.
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#ifndef DXVTOOL_IMAGEIO_H
#define DXVTOOL_IMAGEIO_H

#include <stddef.h>
#include <stdint.h>

/* Loads a PNG / PAM / PPM file as RGBA8 (malloc'd, tightly packed). Returns NULL with a message in
 * err on failure. */
uint8_t *image_load(const char *path, uint32_t *width, uint32_t *height, char *err, size_t err_n);

#endif
