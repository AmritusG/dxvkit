/*
 * DXVKit encoder round trip: for every variant, effort and opcode coding and a set of frame sizes,
 * RGBA -> blocks -> packet -> our decoder must give back exactly the packed block streams, consume the
 * packet exactly (no unused payload, literals or opcodes; every FSE stream leaves 32 bits), and the
 * one-step, two-step and threaded encodes must produce the same bytes. Also writes a MOV and reads it
 * back.
 *
 *   dxvkit_encoder_test [--quick] [--sizes WxH,...] [--out DIR]
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#define _POSIX_C_SOURCE 200809L
#include "dxvkit.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------------------------------- */
/* A jobs runner on plain threads                                                                 */
/* ---------------------------------------------------------------------------------------------- */

typedef struct run_ctx {
    void (*job)(void *, int);
    void *arg;
    int count, stride, first;
} run_ctx;

static void *run_thread(void *p)
{
    run_ctx *c = (run_ctx *)p;
    for (int i = c->first; i < c->count; i += c->stride)
        c->job(c->arg, i);
    return NULL;
}

static void run_jobs(void *user, void (*job)(void *, int), void *arg, int count)
{
    enum { T = 8 };
    pthread_t th[T];
    run_ctx ctx[T];
    int n = count < T ? count : T;
    (void)user;
    for (int t = 0; t < n; t++) {
        ctx[t].job = job, ctx[t].arg = arg, ctx[t].count = count, ctx[t].stride = n, ctx[t].first = t;
        pthread_create(&th[t], NULL, run_thread, &ctx[t]);
    }
    for (int t = 0; t < n; t++)
        pthread_join(th[t], NULL);
}

/* ---------------------------------------------------------------------------------------------- */
/* Test pictures                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

static uint32_t rng(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

/* Flat areas, gradients, a repeated tile, noise, hard edges and an alpha ramp with 0 / 255 regions,
 * shifted by the frame number. */
static void make_picture(uint8_t *rgba, uint32_t w, uint32_t h, int frame)
{
    uint32_t seed = 0x1234567u + (uint32_t)frame * 7919u;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *p = rgba + ((size_t)y * w + x) * 4u;
            uint32_t xs = x + (uint32_t)frame * 3u;
            uint32_t region = (x * 5u / (w ? w : 1u)) + 5u * (y * 3u / (h ? h : 1u));
            switch (region % 6u) {
            case 0: p[0] = 40, p[1] = 90, p[2] = 200; break;                     /* flat */
            case 1: p[0] = (uint8_t)(xs * 3u), p[1] = (uint8_t)(y * 2u), p[2] = 128; break;
            case 2: { /* 16 x 16 tile repeated */
                uint32_t tx = xs & 15u, ty = y & 15u;
                p[0] = (uint8_t)(tx * 16u), p[1] = (uint8_t)(ty * 16u), p[2] = (uint8_t)((tx ^ ty) * 16u);
                break;
            }
            case 3: { /* noise */
                uint32_t r = rng(&seed);
                p[0] = (uint8_t)r, p[1] = (uint8_t)(r >> 8), p[2] = (uint8_t)(r >> 16);
                break;
            }
            case 4: /* hard edges */
                p[0] = p[1] = p[2] = ((xs / 7u + y / 5u) & 1u) ? 255 : 0;
                break;
            default: /* smooth colour */
                p[0] = (uint8_t)(128 + (int)((xs * 7u + y * 3u) % 120u) - 60);
                p[1] = (uint8_t)((y * 255u) / (h ? h : 1u));
                p[2] = (uint8_t)(255u - (xs * 255u) / (w ? w : 1u));
                break;
            }
            if (y < h / 4u)
                p[3] = 255;
            else if (y < h / 2u)
                p[3] = 0;
            else
                p[3] = (uint8_t)((x * 255u) / (w > 1u ? w - 1u : 1u));
        }
    }
}

/* ---------------------------------------------------------------------------------------------- */
/* Round trip                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

typedef struct stats {
    int cases, failures;
} stats;

static const char *variant_name(dxvk_variant v) { return dxvk_variant_name(v); }

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Returns 1 when the case passes. */
static int round_trip(dxvk_variant variant, uint32_t w, uint32_t h, dxvk_effort effort,
                      dxvk_opcode_coding opcodes, uint32_t flags, const uint8_t *rgba, size_t *bytes,
                      double *ms, char *why, size_t why_n)
{
    dxvk_jobs jobs = {run_jobs, NULL};
    dxvk_encode_options opt = {flags, effort, opcodes, NULL}, topt = opt;
    dxvk_encoder *enc = NULL, *tenc = NULL;
    const dxvk_frame_layout *l;
    size_t cap = dxvk_encode_bound(variant, w, h), n1 = 0, n2 = 0, n3 = 0;
    uint8_t *pk1 = malloc(cap), *pk2 = malloc(cap), *pk3 = malloc(cap);
    uint8_t *planes[2] = {NULL, NULL}, *dec[2] = {NULL, NULL};
    dxvk_consumption c;
    dxvk_frame_layout dl;
    int ok = 0;
    double t0;
    dxvk_status st;
    topt.jobs = &jobs;
    why[0] = 0;

    if ((st = dxvk_encoder_create(variant, w, h, &opt, &enc)) != DXVK_OK ||
        (st = dxvk_encoder_create(variant, w, h, &topt, &tenc)) != DXVK_OK) {
        snprintf(why, why_n, "create: %s", dxvk_status_string(st));
        goto done;
    }
    l = dxvk_encoder_layout(enc);
    for (int i = 0; i < l->plane_count; i++) {
        planes[i] = malloc(l->bytes[i]);
        dec[i] = malloc(l->bytes[i]);
    }
    /* two-step: compress, then pack */
    {
        void *pp[2] = {planes[0], planes[1]};
        const void *cp[2] = {planes[0], planes[1]};
        t0 = now();
        if ((st = dxvk_encoder_compress(enc, rgba, (size_t)w * 4u, pp, l->bytes)) != DXVK_OK ||
            (st = dxvk_encoder_pack(enc, cp, pk1, cap, &n1)) != DXVK_OK) {
            snprintf(why, why_n, "encode: %s", dxvk_status_string(st));
            goto done;
        }
        *ms = (now() - t0) * 1000.0;
    }
    /* one-step and threaded */
    if ((st = dxvk_encoder_encode(enc, rgba, (size_t)w * 4u, pk2, cap, &n2)) != DXVK_OK ||
        (st = dxvk_encoder_encode(tenc, rgba, (size_t)w * 4u, pk3, cap, &n3)) != DXVK_OK) {
        snprintf(why, why_n, "encode: %s", dxvk_status_string(st));
        goto done;
    }
    if (n1 != n2 || memcmp(pk1, pk2, n1) != 0) {
        snprintf(why, why_n, "one-step encode differs from compress + pack");
        goto done;
    }
    if (n1 != n3 || memcmp(pk1, pk3, n1) != 0) {
        snprintf(why, why_n, "threaded encode differs");
        goto done;
    }
    for (int i = 0; i < l->plane_count; i++)
        if (memcmp(dxvk_encoder_plane(enc, i), planes[i], l->bytes[i]) != 0) {
            snprintf(why, why_n, "dxvk_encoder_plane differs");
            goto done;
        }
    /* decode */
    {
        void *dp[2] = {dec[0], dec[1]};
        st = dxvk_decode_blocks(DXVK_GEN_DXV3, pk1, n1, w, h, NULL, dp, l->bytes, &dl, &c);
        if (st != DXVK_OK) {
            snprintf(why, why_n, "decode: %s", dxvk_status_string(st));
            goto done;
        }
    }
    if (dl.variant != variant) {
        snprintf(why, why_n, "decoded variant %s", variant_name(dl.variant));
        goto done;
    }
    for (int i = 0; i < l->plane_count; i++)
        if (memcmp(dec[i], planes[i], l->bytes[i]) != 0) {
            snprintf(why, why_n, "plane %d differs after decode", i);
            goto done;
        }
    if (c.payload_unused || c.literals_unused || c.opcodes_unused ||
        (c.fse_streams && (c.fse_bits_left_min != 32 || c.fse_bits_left_max != 32))) {
        snprintf(why, why_n, "not exactly consumed: payload %zu literals %zu opcodes %u fse %u..%u",
                 c.payload_unused, c.literals_unused, c.opcodes_unused, c.fse_bits_left_min,
                 c.fse_bits_left_max);
        goto done;
    }
    if (n1 > cap) {
        snprintf(why, why_n, "packet larger than the bound");
        goto done;
    }
    if (opcodes == DXVK_OPCODES_STORED && c.fse_streams) {
        snprintf(why, why_n, "FSE stream with opcodes = stored");
        goto done;
    }
    *bytes = n1;
    ok = 1;
done:
    dxvk_encoder_destroy(enc);
    dxvk_encoder_destroy(tenc);
    free(pk1);
    free(pk2);
    free(pk3);
    for (int i = 0; i < 2; i++) {
        free(planes[i]);
        free(dec[i]);
    }
    return ok;
}

static int mov_test(const char *dir)
{
    char path[1024];
    dxvk_encoder *enc;
    dxvk_mov_writer *mw;
    dxvk_mov *mov;
    const dxvk_mov_info *in;
    uint32_t w = 333, h = 221;
    size_t cap = dxvk_encode_bound(DXVK_VARIANT_YG10, w, h), n;
    uint8_t *rgba = malloc((size_t)w * h * 4u), *pk[3], *back = malloc(cap);
    int ok = 1;
    snprintf(path, sizeof path, "%s/encoder_test_yg10_333x221.mov", dir);
    if (dxvk_encoder_create(DXVK_VARIANT_YG10, w, h, NULL, &enc) != DXVK_OK ||
        dxvk_mov_writer_open_file(path, w, h, 30000, 1001, &mw) != DXVK_OK) {
        fprintf(stderr, "mov: cannot create %s\n", path);
        return 0;
    }
    size_t sizes[3];
    for (int f = 0; f < 3; f++) {
        pk[f] = malloc(cap);
        make_picture(rgba, w, h, f);
        ok = ok && dxvk_encoder_encode(enc, rgba, (size_t)w * 4u, pk[f], cap, &sizes[f]) == DXVK_OK &&
             dxvk_mov_writer_add(mw, pk[f], sizes[f]) == DXVK_OK;
    }
    ok = ok && dxvk_mov_writer_finish(mw) == DXVK_OK;
    dxvk_mov_writer_close(mw);
    dxvk_encoder_destroy(enc);
    if (ok && dxvk_mov_open_file(path, &mov) == DXVK_OK) {
        in = dxvk_mov_get_info(mov);
        ok = in->width == w && in->height == h && in->frame_count == 3 && in->timescale == 30000 &&
             in->frame_duration == 1001 && in->fourcc == 0x44584433u && in->generation == DXVK_GEN_DXV3;
        for (uint32_t f = 0; f < 3 && ok; f++)
            ok = dxvk_mov_read_packet(mov, f, back, cap, &n) == DXVK_OK && n == sizes[f] &&
                 memcmp(back, pk[f], n) == 0;
        dxvk_mov_close(mov);
    } else {
        ok = 0;
    }
    printf("mov writer: %s -> reopen, 3 packets identical, 29.97 fps, DXD3: %s\n", path,
           ok ? "ok" : "FAIL");
    for (int f = 0; f < 3; f++)
        free(pk[f]);
    free(rgba);
    free(back);
    return ok;
}

int main(int argc, char **argv)
{
    static const uint32_t all_sizes[][2] = {{16, 16},   {18, 10},   {333, 221}, {1920, 1080},
                                            {908, 2276}, {3840, 2160}, {1, 1}};
    static const dxvk_variant variants[] = {DXVK_VARIANT_DXT1, DXVK_VARIANT_DXT5, DXVK_VARIANT_YCG6,
                                            DXVK_VARIANT_YG10};
    uint32_t sizes[32][2];
    int nsizes = 0, quick = 0;
    const char *out_dir = ".";
    stats s = {0, 0};
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "--quick") == 0) {
            quick = 1;
        } else if (strcmp(argv[a], "--out") == 0 && a + 1 < argc) {
            out_dir = argv[++a];
        } else if (strcmp(argv[a], "--sizes") == 0 && a + 1 < argc) {
            char *p = argv[++a];
            while (*p && nsizes < 32) {
                sizes[nsizes][0] = (uint32_t)strtoul(p, &p, 10);
                if (*p == 'x')
                    p++;
                sizes[nsizes][1] = (uint32_t)strtoul(p, &p, 10);
                nsizes++;
                if (*p == ',')
                    p++;
            }
        }
    }
    if (nsizes == 0)
        for (int i = 0; i < 7; i++) {
            if (quick && all_sizes[i][0] * all_sizes[i][1] > 2000000u)
                continue;
            sizes[nsizes][0] = all_sizes[i][0];
            sizes[nsizes][1] = all_sizes[i][1];
            nsizes++;
        }

    printf("%-6s %-11s %-5s %-7s %-6s %12s %10s  %s\n", "var", "size", "eff", "opcodes", "flags",
           "packet B", "ms", "result");
    for (int si = 0; si < nsizes; si++) {
        uint32_t w = sizes[si][0], h = sizes[si][1];
        uint8_t *rgba = malloc((size_t)w * h * 4u);
        make_picture(rgba, w, h, si);
        for (int vi = 0; vi < 4; vi++) {
            dxvk_variant v = variants[vi];
            int hq = v == DXVK_VARIANT_YCG6 || v == DXVK_VARIANT_YG10;
            for (int eff = 0; eff < 2; eff++) {
                for (int mode = 0; mode < (hq ? 2 : 2); mode++) {
                    /* HQ: opcode coding auto / stored; DXT1 / DXT5: compressed / stored payload */
                    dxvk_opcode_coding oc = hq && mode ? DXVK_OPCODES_STORED : DXVK_OPCODES_AUTO;
                    uint32_t flags = !hq && mode ? DXVK_ENC_STORED_PAYLOAD : 0;
                    char why[256], sz[32];
                    size_t bytes = 0;
                    double ms = 0;
                    int ok = round_trip(v, w, h, (dxvk_effort)eff, oc, flags, rgba, &bytes, &ms, why,
                                        sizeof why);
                    s.cases++;
                    if (!ok)
                        s.failures++;
                    snprintf(sz, sizeof sz, "%ux%u", w, h);
                    printf("%-6s %-11s %-5s %-7s %-6s %12zu %10.2f  %s%s\n", variant_name(v), sz,
                           eff ? "best" : "fast", hq ? (mode ? "stored" : "auto") : "-",
                           !hq && mode ? "stored" : "-", bytes, ms, ok ? "ok" : "FAIL: ", why);
                }
            }
        }
        free(rgba);
    }
    if (!mov_test(out_dir))
        s.failures++;
    printf("\nround trip: %d cases, %d failures\n", s.cases, s.failures);
    return s.failures ? 1 : 0;
}
