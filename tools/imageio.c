/*
 * dxvtool image input. PNG per the PNG specification (chunks, filters 0-4, colour types 0 / 2 / 3 / 4 /
 * 6, bit depth 8 or 16, no interlacing), inflated by the system zlib (zlib licence). PAM (P7) and
 * binary PPM (P6).
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#include "imageio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint8_t *read_all(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *p = NULL;
    size_t cap = 0, len = 0;
    if (!f)
        return NULL;
    for (;;) {
        size_t got;
        if (cap - len < 65536) {
            uint8_t *q;
            cap = cap ? cap * 2 : 1 << 20;
            q = (uint8_t *)realloc(p, cap);
            if (!q) {
                free(p);
                fclose(f);
                return NULL;
            }
            p = q;
        }
        got = fread(p + len, 1, cap - len, f);
        len += got;
        if (got == 0)
            break;
    }
    fclose(f);
    *n = len;
    return p;
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c;
}

static uint8_t *load_png(const uint8_t *d, size_t n, uint32_t *w, uint32_t *h, char *err, size_t en)
{
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    uint32_t width = 0, height = 0;
    int depth = 0, ctype = -1, channels, bpp;
    uint8_t palette[256][4];
    uint8_t *idat = NULL, *raw = NULL, *out = NULL;
    size_t idat_n = 0, pos = 8, stride;
    uLongf raw_n;
    memset(palette, 255, sizeof palette);
    if (n < 8 || memcmp(d, sig, 8) != 0) {
        snprintf(err, en, "not a PNG");
        return NULL;
    }
    while (pos + 12 <= n) {
        uint32_t len = be32(d + pos);
        const uint8_t *type = d + pos + 4, *body = d + pos + 8;
        if (len > n - pos - 12) {
            snprintf(err, en, "truncated PNG chunk");
            goto fail;
        }
        if (memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            width = be32(body);
            height = be32(body + 4);
            depth = body[8];
            ctype = body[9];
            if (body[12] != 0) {
                snprintf(err, en, "interlaced PNG not supported");
                goto fail;
            }
        } else if (memcmp(type, "PLTE", 4) == 0) {
            for (uint32_t i = 0; i < len / 3 && i < 256; i++)
                palette[i][0] = body[3 * i], palette[i][1] = body[3 * i + 1],
                palette[i][2] = body[3 * i + 2];
        } else if (memcmp(type, "tRNS", 4) == 0 && ctype == 3) {
            for (uint32_t i = 0; i < len && i < 256; i++)
                palette[i][3] = body[i];
        } else if (memcmp(type, "IDAT", 4) == 0) {
            uint8_t *q = (uint8_t *)realloc(idat, idat_n + len);
            if (!q)
                goto fail;
            idat = q;
            memcpy(idat + idat_n, body, len);
            idat_n += len;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12u + len;
    }
    if (width == 0 || height == 0 || width > 65535 || height > 65535 ||
        (depth != 8 && depth != 16) || (ctype == 3 && depth != 8)) {
        snprintf(err, en, "unsupported PNG (size %ux%u, depth %d, colour type %d)", width, height,
                 depth, ctype);
        goto fail;
    }
    switch (ctype) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default: snprintf(err, en, "bad PNG colour type"); goto fail;
    }
    bpp = channels * depth / 8;
    stride = (size_t)width * (size_t)bpp;
    raw_n = (uLongf)((stride + 1) * height);
    raw = (uint8_t *)malloc(raw_n);
    out = (uint8_t *)malloc((size_t)width * height * 4u);
    if (!raw || !out)
        goto fail;
    if (uncompress(raw, &raw_n, idat, (uLong)idat_n) != Z_OK || raw_n != (stride + 1) * height) {
        snprintf(err, en, "PNG data does not inflate to the image size");
        goto fail;
    }
    for (uint32_t y = 0; y < height; y++) {
        uint8_t *row = raw + y * (stride + 1), *cur = row + 1;
        const uint8_t *up = y ? raw + (y - 1) * (stride + 1) + 1 : NULL;
        int filter = row[0];
        for (size_t i = 0; i < stride; i++) {
            int a = i >= (size_t)bpp ? cur[i - bpp] : 0, b = up ? up[i] : 0,
                c = (up && i >= (size_t)bpp) ? up[i - bpp] : 0, v = cur[i];
            switch (filter) {
            case 0: break;
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: v += paeth(a, b, c); break;
            default: snprintf(err, en, "bad PNG filter"); goto fail;
            }
            cur[i] = (uint8_t)v;
        }
        for (uint32_t x = 0; x < width; x++) {
            uint8_t s[4];
            uint8_t *dst = out + ((size_t)y * width + x) * 4u;
            for (int k = 0; k < channels; k++)
                s[k] = cur[(size_t)x * bpp + (size_t)k * (depth / 8)]; /* 16-bit: high byte */
            switch (ctype) {
            case 0: dst[0] = dst[1] = dst[2] = s[0], dst[3] = 255; break;
            case 2: dst[0] = s[0], dst[1] = s[1], dst[2] = s[2], dst[3] = 255; break;
            case 3: memcpy(dst, palette[s[0]], 4); break;
            case 4: dst[0] = dst[1] = dst[2] = s[0], dst[3] = s[1]; break;
            default: memcpy(dst, s, 4); break;
            }
        }
    }
    free(idat);
    free(raw);
    *w = width;
    *h = height;
    return out;
fail:
    free(idat);
    free(raw);
    free(out);
    return NULL;
}

/* PAM (P7, TUPLTYPE RGB / RGB_ALPHA / GRAYSCALE, MAXVAL 255) or PPM (P6, maxval 255). */
static uint8_t *load_pnm(const uint8_t *d, size_t n, uint32_t *w, uint32_t *h, char *err, size_t en)
{
    uint32_t width = 0, height = 0, depth = 3, maxval = 255;
    size_t pos = 2;
    uint8_t *out;
    if (d[1] == '7') {
        char line[256];
        for (;;) {
            size_t k = 0;
            while (pos < n && d[pos] != '\n' && k < sizeof line - 1)
                line[k++] = (char)d[pos++];
            line[k] = 0;
            pos++;
            if (pos > n) {
                snprintf(err, en, "truncated PAM header");
                return NULL;
            }
            if (strncmp(line, "WIDTH ", 6) == 0)
                width = (uint32_t)strtoul(line + 6, NULL, 10);
            else if (strncmp(line, "HEIGHT ", 7) == 0)
                height = (uint32_t)strtoul(line + 7, NULL, 10);
            else if (strncmp(line, "DEPTH ", 6) == 0)
                depth = (uint32_t)strtoul(line + 6, NULL, 10);
            else if (strncmp(line, "MAXVAL ", 7) == 0)
                maxval = (uint32_t)strtoul(line + 7, NULL, 10);
            else if (strcmp(line, "ENDHDR") == 0)
                break;
        }
    } else {
        uint32_t vals[3];
        for (int i = 0; i < 3; i++) {
            while (pos < n && (d[pos] == ' ' || d[pos] == '\n' || d[pos] == '\r' || d[pos] == '\t' ||
                               d[pos] == '#')) {
                if (d[pos] == '#')
                    while (pos < n && d[pos] != '\n')
                        pos++;
                else
                    pos++;
            }
            vals[i] = 0;
            while (pos < n && d[pos] >= '0' && d[pos] <= '9')
                vals[i] = vals[i] * 10u + (uint32_t)(d[pos++] - '0');
        }
        pos++;
        width = vals[0], height = vals[1], maxval = vals[2];
    }
    if (width == 0 || height == 0 || maxval != 255 || (depth != 1 && depth != 3 && depth != 4) ||
        (size_t)width * height * depth > n - pos) {
        snprintf(err, en, "unsupported or truncated PAM / PPM");
        return NULL;
    }
    out = (uint8_t *)malloc((size_t)width * height * 4u);
    if (!out)
        return NULL;
    for (size_t i = 0; i < (size_t)width * height; i++) {
        const uint8_t *s = d + pos + i * depth;
        uint8_t *o = out + i * 4u;
        o[0] = s[0];
        o[1] = depth >= 3 ? s[1] : s[0];
        o[2] = depth >= 3 ? s[2] : s[0];
        o[3] = depth == 4 ? s[3] : 255;
    }
    *w = width;
    *h = height;
    return out;
}

uint8_t *image_load(const char *path, uint32_t *width, uint32_t *height, char *err, size_t err_n)
{
    size_t n = 0;
    uint8_t *d = read_all(path, &n), *img = NULL;
    if (!d) {
        snprintf(err, err_n, "cannot read %s", path);
        return NULL;
    }
    if (n >= 8 && d[0] == 0x89 && d[1] == 'P')
        img = load_png(d, n, width, height, err, err_n);
    else if (n >= 3 && d[0] == 'P' && (d[1] == '7' || d[1] == '6'))
        img = load_pnm(d, n, width, height, err, err_n);
    else
        snprintf(err, err_n, "%s: not a PNG, PAM or PPM file", path);
    free(d);
    return img;
}
