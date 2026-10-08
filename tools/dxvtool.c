/*
 * dxvtool: command-line front end for DXVKit.
 *
 *   dxvtool info FILE...                        container + packet summary
 *   dxvtool dump-blocks FILE FRAME OUTPREFIX    block stream(s) of one frame
 *   dxvtool dump-rgba FILE FRAME OUT [--premultiply]
 *   dxvtool verify VECTORS.TSV [--threads N]    every test-vector row, byte-exact
 *   dxvtool check FILE... [--threads N] [--histogram]   decode every packet, report byte consumption
 *   dxvtool bench FILE [--threads N] [--seconds S] [--frames N]
 *   dxvtool encode IN... OUT.mov [--variant V] [--effort E] [--size WxH] [--fps F] [--threads N] ...
 *   dxvtool bench-encode SOURCE.rgba --size WxH [--variant V] [--effort E] [--threads N]
 *   dxvtool repack FILE.mov OUT.mov [--effort E] [--opcodes O]   same blocks, DXVKit's packer
 *   dxvtool psnr FILE.mov SOURCE.rgba [--frames N]   decoded RGBA vs a raw RGBA source
 *   dxvtool cmp-rgba A.rgba B.rgba --size WxH [--unpremultiply-b]   byte differences of two RGBA files
 *   dxvtool dump-samples FILE FRAME OUT              HQ: decoded Y, Co, Cg (, A) sample planes
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _DEFAULT_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "dxvkit.h"
#include "imageio.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

/* ---------------------------------------------------------------------------------------------- */
/* SHA-256 (FIPS 180-4)                                                                           */
/* ---------------------------------------------------------------------------------------------- */

typedef struct sha256 {
    uint32_t h[8];
    uint64_t length;
    uint8_t block[64];
    size_t fill;
} sha256;

static const uint32_t sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_compress(sha256 *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
    e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha_k[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
    }
    s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d;
    s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

static void sha_init(sha256 *s)
{
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, iv, sizeof iv);
    s->length = 0;
    s->fill = 0;
}

static void sha_update(sha256 *s, const uint8_t *p, size_t n)
{
    s->length += n;
    while (n > 0) {
        size_t take = 64 - s->fill;
        if (take > n)
            take = n;
        memcpy(s->block + s->fill, p, take);
        s->fill += take;
        p += take;
        n -= take;
        if (s->fill == 64) {
            sha_compress(s, s->block);
            s->fill = 0;
        }
    }
}

static void sha_hex(const uint8_t *p, size_t n, char out[65])
{
    sha256 s;
    uint8_t pad[72];
    uint64_t bits;
    size_t padn;
    sha_init(&s);
    sha_update(&s, p, n);
    bits = s.length * 8u;
    padn = (s.fill < 56 ? 56 - s.fill : 120 - s.fill);
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++)
        pad[padn + i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_update(&s, pad, padn + 8);
    for (int i = 0; i < 8; i++)
        sprintf(out + 8 * i, "%08x", s.h[i]);
}

/* ---------------------------------------------------------------------------------------------- */
/* Job pool (the tool's implementation of dxvk_jobs)                                              */
/* ---------------------------------------------------------------------------------------------- */

typedef struct pool {
    pthread_mutex_t lock;
    pthread_cond_t wake, done;
    pthread_t *threads;
    int nthreads;
    unsigned generation;
    int quit;
    void (*job)(void *, int);
    void *arg;
    int count, next, finished;
} pool;

static void pool_work(pool *p)
{
    for (;;) {
        int i;
        if (p->next >= p->count)
            return;
        i = p->next++;
        pthread_mutex_unlock(&p->lock);
        p->job(p->arg, i);
        pthread_mutex_lock(&p->lock);
        if (++p->finished == p->count)
            pthread_cond_broadcast(&p->done);
    }
}

static void *pool_thread(void *arg)
{
    pool *p = (pool *)arg;
    unsigned seen = 0;
    pthread_mutex_lock(&p->lock);
    for (;;) {
        while (!p->quit && p->generation == seen)
            pthread_cond_wait(&p->wake, &p->lock);
        if (p->quit)
            break;
        seen = p->generation;
        pool_work(p);
    }
    pthread_mutex_unlock(&p->lock);
    return NULL;
}

static void pool_run(void *user, void (*job)(void *, int), void *arg, int count)
{
    pool *p = (pool *)user;
    pthread_mutex_lock(&p->lock);
    p->job = job;
    p->arg = arg;
    p->count = count;
    p->next = 0;
    p->finished = 0;
    p->generation++;
    pthread_cond_broadcast(&p->wake);
    pool_work(p);
    while (p->finished < p->count)
        pthread_cond_wait(&p->done, &p->lock);
    pthread_mutex_unlock(&p->lock);
}

static pool *pool_create(int nthreads)
{
    pool *p = (pool *)calloc(1, sizeof *p);
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->wake, NULL);
    pthread_cond_init(&p->done, NULL);
    p->nthreads = nthreads - 1; /* the caller works too */
    p->threads = (pthread_t *)calloc((size_t)(p->nthreads > 0 ? p->nthreads : 1), sizeof(pthread_t));
    for (int i = 0; i < p->nthreads; i++)
        pthread_create(&p->threads[i], NULL, pool_thread, p);
    return p;
}

static void pool_destroy(pool *p)
{
    if (!p)
        return;
    pthread_mutex_lock(&p->lock);
    p->quit = 1;
    pthread_cond_broadcast(&p->wake);
    pthread_mutex_unlock(&p->lock);
    for (int i = 0; i < p->nthreads; i++)
        pthread_join(p->threads[i], NULL);
    free(p->threads);
    free(p);
}

/* ---------------------------------------------------------------------------------------------- */
/* Helpers                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

typedef struct frame_buffers {
    uint8_t *packet;
    size_t packet_cap;
    uint8_t *plane[2];
    size_t plane_cap[2];
    uint8_t *rgba;
    size_t rgba_cap;
} frame_buffers;

static int reserve(uint8_t **p, size_t *cap, size_t need)
{
    if (*cap >= need)
        return 1;
    free(*p);
    *p = (uint8_t *)malloc(need ? need : 1);
    *cap = *p ? need : 0;
    return *p != NULL;
}

static void buffers_free(frame_buffers *b)
{
    free(b->packet);
    free(b->plane[0]);
    free(b->plane[1]);
    free(b->rgba);
}

/* Reads and decodes frame `index` into b. */
static dxvk_status decode_frame(dxvk_mov *mov, uint32_t index, const dxvk_options *opt,
                                frame_buffers *b, dxvk_frame_layout *layout, dxvk_consumption *cons,
                                size_t *packet_size)
{
    const dxvk_mov_info *info = dxvk_mov_get_info(mov);
    dxvk_frame_layout big;
    size_t n;
    dxvk_status st;
    void *planes[2];
    /* the largest variant at this size: YG10 / DXT5 need the most */
    if ((st = dxvk_layout(DXVK_VARIANT_YG10, info->width, info->height, &big)) != DXVK_OK)
        return st;
    if (!reserve(&b->packet, &b->packet_cap, info->max_packet_size) ||
        !reserve(&b->plane[0], &b->plane_cap[0], big.bytes[0]) ||
        !reserve(&b->plane[1], &b->plane_cap[1], big.bytes[1]))
        return DXVK_ERR_OUT_OF_MEMORY;
    if ((st = dxvk_mov_read_packet(mov, index, b->packet, b->packet_cap, &n)) != DXVK_OK)
        return st;
    if (packet_size)
        *packet_size = n;
    planes[0] = b->plane[0];
    planes[1] = b->plane[1];
    return dxvk_decode_blocks(info->generation, b->packet, n, info->width, info->height, opt,
                              planes, b->plane_cap, layout, cons);
}

static dxvk_status to_rgba(const dxvk_frame_layout *layout, const dxvk_options *opt,
                           frame_buffers *b)
{
    const void *planes[2];
    size_t need = (size_t)layout->display_width * layout->display_height * 4u;
    if (!reserve(&b->rgba, &b->rgba_cap, need))
        return DXVK_ERR_OUT_OF_MEMORY;
    planes[0] = b->plane[0];
    planes[1] = b->plane[1];
    return dxvk_blocks_to_rgba(layout, planes, opt, b->rgba, (size_t)layout->display_width * 4u);
}

static const char *plane_suffix(const dxvk_frame_layout *l, int plane)
{
    if (plane == 1)
        return "chroma.bc5";
    switch (l->format[0]) {
    case DXVK_BLOCKS_BC1: return "bc1";
    case DXVK_BLOCKS_BC3: return "bc3";
    case DXVK_BLOCKS_BC4: return "y.bc4";
    default: return "ya.bc5";
    }
}

static int write_file(const char *path, const void *p, size_t n)
{
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(p, 1, n, f) == n;
    if (f && fclose(f) != 0)
        ok = 0;
    if (!ok)
        fprintf(stderr, "cannot write %s\n", path);
    return ok;
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int default_threads(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

static int open_or_report(const char *path, dxvk_mov **mov)
{
    dxvk_status st = dxvk_mov_open_file(path, mov);
    if (st != DXVK_OK) {
        fprintf(stderr, "%s: %s\n", path, dxvk_status_string(st));
        return 0;
    }
    return 1;
}

static const char *fourcc_str(uint32_t f, char out[5])
{
    for (int i = 0; i < 4; i++) {
        char c = (char)(f >> (24 - 8 * i));
        out[i] = (c >= 32 && c < 127) ? c : '?';
    }
    out[4] = 0;
    return out;
}

/* ---------------------------------------------------------------------------------------------- */
/* Commands                                                                                       */
/* ---------------------------------------------------------------------------------------------- */

static int cmd_info(int argc, char **argv)
{
    int rc = 0;
    for (int a = 0; a < argc; a++) {
        dxvk_mov *mov;
        const dxvk_mov_info *in;
        dxvk_frame_layout l;
        uint32_t counts[6] = {0};
        uint32_t bad = 0;
        frame_buffers b;
        char fcc[5];
        memset(&b, 0, sizeof b);
        if (!open_or_report(argv[a], &mov)) {
            rc = 1;
            continue;
        }
        in = dxvk_mov_get_info(mov);
        for (uint32_t i = 0; i < in->frame_count; i++) {
            size_t n;
            dxvk_variant v;
            if (!reserve(&b.packet, &b.packet_cap, in->max_packet_size) ||
                dxvk_mov_read_packet(mov, i, b.packet, b.packet_cap, &n) != DXVK_OK ||
                dxvk_packet_variant(in->generation, b.packet, n, &v, NULL) != DXVK_OK)
                bad++;
            else
                counts[v]++;
        }
        printf("%s\n  fourcc %s  %ux%u  frames %u  %.4g fps  max packet %u bytes  track %u\n",
               argv[a], fourcc_str(in->fourcc, fcc), in->width, in->height, in->frame_count,
               in->frame_rate, in->max_packet_size, in->track_id);
        for (int v = 0; v < 6; v++) {
            if (!counts[v])
                continue;
            dxvk_layout((dxvk_variant)v, in->width, in->height, &l);
            printf("  variant %-11s packets %u  alpha %s  coded %ux%u  planes %d (%zu + %zu bytes)\n",
                   dxvk_variant_name((dxvk_variant)v), counts[v],
                   dxvk_variant_has_alpha((dxvk_variant)v) ? "yes" : "no", l.coded_width,
                   l.coded_height, l.plane_count, l.bytes[0], l.bytes[1]);
        }
        if (bad) {
            printf("  unreadable packet headers: %u\n", bad);
            rc = 1;
        }
        buffers_free(&b);
        dxvk_mov_close(mov);
    }
    return rc;
}

static int cmd_dump(int argc, char **argv, int rgba)
{
    dxvk_mov *mov;
    frame_buffers b;
    dxvk_frame_layout l;
    dxvk_options opt;
    dxvk_status st;
    char path[4096];
    int ok = 1;
    if (argc < 3)
        return 2;
    memset(&b, 0, sizeof b);
    memset(&opt, 0, sizeof opt);
    if (argc > 3 && strcmp(argv[3], "--premultiply") == 0)
        opt.flags |= DXVK_OPT_PREMULTIPLY;
    if (!open_or_report(argv[0], &mov))
        return 1;
    st = decode_frame(mov, (uint32_t)strtoul(argv[1], NULL, 10), &opt, &b, &l, NULL, NULL);
    if (st == DXVK_OK && rgba)
        st = to_rgba(&l, &opt, &b);
    if (st != DXVK_OK) {
        fprintf(stderr, "%s frame %s: %s\n", argv[0], argv[1], dxvk_status_string(st));
        ok = 0;
    } else if (rgba) {
        ok = write_file(argv[2], b.rgba, (size_t)l.display_width * l.display_height * 4u);
        printf("%s: %ux%u RGBA8\n", argv[2], l.display_width, l.display_height);
    } else {
        for (int i = 0; i < l.plane_count && ok; i++) {
            snprintf(path, sizeof path, "%s.%s", argv[2], plane_suffix(&l, i));
            ok = write_file(path, b.plane[i], l.bytes[i]);
            printf("%s: %zu bytes (%ux%u blocks)\n", path, l.bytes[i], l.blocks_x[i], l.blocks_y[i]);
        }
    }
    buffers_free(&b);
    dxvk_mov_close(mov);
    return ok ? 0 : 1;
}

/* Finds "key=hex" in a space-separated list. */
static int find_hash(const char *list, const char *key, char out[65])
{
    size_t kl = strlen(key);
    const char *p = list;
    while ((p = strstr(p, key)) != NULL) {
        if ((p == list || p[-1] == ' ') && p[kl] == '=') {
            memcpy(out, p + kl + 1, 64);
            out[64] = 0;
            return 1;
        }
        p += kl;
    }
    return 0;
}

static int cmd_verify(int argc, char **argv)
{
    char dir[4096], line[8192], path[8192];
    const char *slash;
    FILE *f;
    pool *pl = NULL;
    dxvk_jobs jobs;
    dxvk_options opt;
    int threads = 1;
    int rows = 0, streams_ok = 0, streams = 0, rgba_ok = 0, rgba_n = 0, rej_ok = 0, rej_n = 0;
    int failures = 0;
    frame_buffers b;

    if (argc < 1)
        return 2;
    for (int a = 1; a + 1 < argc; a++)
        if (strcmp(argv[a], "--threads") == 0)
            threads = atoi(argv[a + 1]);
    memset(&opt, 0, sizeof opt);
    memset(&b, 0, sizeof b);
    if (threads > 1) {
        pl = pool_create(threads);
        jobs.run = pool_run;
        jobs.user = pl;
        opt.jobs = &jobs;
    }
    slash = strrchr(argv[0], '/');
    if (slash)
        snprintf(dir, sizeof dir, "%.*s", (int)(slash - argv[0]), argv[0]);
    else
        snprintf(dir, sizeof dir, ".");
    f = fopen(argv[0], "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[0]);
        return 1;
    }
    printf("%-42s %-9s %3s %-6s %-11s %-9s %-6s %-6s %s\n", "vector", "group", "frm", "expect",
           "variant", "coded", "planes", "rgba", "result");
    while (fgets(line, sizeof line, f)) {
        char *col[10];
        int n = 0;
        char *s = line, *e;
        dxvk_mov *mov = NULL;
        dxvk_frame_layout l;
        dxvk_status st;
        char coded[32] = "-", planes_res[16] = "-", rgba_res[16] = "-", result[160];
        int decode_row, row_ok = 1;
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if ((e = strchr(line, '\n')) != NULL)
            *e = 0;
        while (n < 10) {
            col[n++] = s;
            e = strchr(s, '\t');
            if (!e)
                break;
            *e = 0;
            s = e + 1;
        }
        if (n < 9)
            continue;
        if (n < 10)
            col[9] = "";
        rows++;
        decode_row = strcmp(col[8], "decode") == 0;
        snprintf(path, sizeof path, "%s/streams/%s/%s.mov", dir, col[1], col[0]);
        st = dxvk_mov_open_file(path, &mov);
        if (st == DXVK_OK)
            st = decode_frame(mov, (uint32_t)atoi(col[2]), &opt, &b, &l, NULL, NULL);
        if (decode_row) {
            if (st != DXVK_OK) {
                snprintf(result, sizeof result, "FAIL: %s", dxvk_status_string(st));
                row_ok = 0;
                streams += 1;
                rgba_n++;
            } else {
                char want[65], got[65];
                int planes_good = 0;
                const char *key0 = l.format[0] == DXVK_BLOCKS_BC1   ? "bc1"
                                   : l.format[0] == DXVK_BLOCKS_BC3 ? "bc3"
                                                                    : "plane-a";
                snprintf(coded, sizeof coded, "%ux%u", l.coded_width, l.coded_height);
                if (strcmp(coded, col[5]) != 0)
                    row_ok = 0;
                for (int i = 0; i < l.plane_count; i++) {
                    streams++;
                    sha_hex(b.plane[i], l.bytes[i], got);
                    if (find_hash(col[9], i == 0 ? key0 : "plane-b", want) &&
                        strcmp(want, got) == 0) {
                        planes_good++;
                        streams_ok++;
                    } else {
                        row_ok = 0;
                    }
                }
                snprintf(planes_res, sizeof planes_res, "%d/%d", planes_good, l.plane_count);
                rgba_n++;
                st = to_rgba(&l, &opt, &b);
                if (st == DXVK_OK) {
                    sha_hex(b.rgba, (size_t)l.display_width * l.display_height * 4u, got);
                    if (find_hash(col[9], "rgba", want) && strcmp(want, got) == 0) {
                        rgba_ok++;
                        snprintf(rgba_res, sizeof rgba_res, "ok");
                    } else {
                        row_ok = 0;
                        snprintf(rgba_res, sizeof rgba_res, "DIFF");
                    }
                } else {
                    row_ok = 0;
                    snprintf(rgba_res, sizeof rgba_res, "ERR");
                }
                snprintf(result, sizeof result, row_ok ? "ok" : "FAIL");
            }
            printf("%-42s %-9s %3s %-6s %-11s %-9s %-6s %-6s %s\n", col[0], col[1], col[2], "decode",
                   st == DXVK_OK || row_ok ? dxvk_variant_name(l.variant) : "-", coded, planes_res,
                   rgba_res, result);
        } else {
            rej_n++;
            if (st != DXVK_OK) {
                rej_ok++;
                snprintf(result, sizeof result, "rejected: %s", dxvk_status_string(st));
            } else {
                row_ok = 0;
                snprintf(result, sizeof result, "FAIL: decoded");
            }
            printf("%-42s %-9s %3s %-6s %-11s %-9s %-6s %-6s %s\n", col[0], col[1], col[2], "reject",
                   "-", "-", "-", "-", result);
        }
        if (!row_ok)
            failures++;
        dxvk_mov_close(mov);
    }
    fclose(f);
    printf("\nrows %d | block streams %d/%d identical | rgba %d/%d identical | rejected %d/%d | "
           "failures %d\n",
           rows, streams_ok, streams, rgba_ok, rgba_n, rej_ok, rej_n, failures);
    buffers_free(&b);
    pool_destroy(pl);
    return failures ? 1 : 0;
}

static int cmd_check(int argc, char **argv)
{
    int threads = 1, files = 0, files_bad = 0, show_histogram = 0;
    unsigned long long packets = 0, errors = 0, inexact = 0, tails = 0;
    pool *pl = NULL;
    dxvk_jobs jobs;
    dxvk_options opt;
    frame_buffers b;
    memset(&opt, 0, sizeof opt);
    memset(&b, 0, sizeof b);
    for (int a = 0; a + 1 < argc; a++)
        if (strcmp(argv[a], "--threads") == 0)
            threads = atoi(argv[a + 1]);
    for (int a = 0; a < argc; a++)
        if (strcmp(argv[a], "--histogram") == 0)
            show_histogram = 1;
    if (threads > 1) {
        pl = pool_create(threads);
        jobs.run = pool_run;
        jobs.user = pl;
        opt.jobs = &jobs;
    }
    for (int a = 0; a < argc; a++) {
        dxvk_mov *mov;
        const dxvk_mov_info *in;
        uint32_t err = 0, notexact = 0, tail = 0, fse = 0, fse_min = UINT32_MAX, fse_max = 0;
        unsigned long long lit_unused = 0, ops_unused = 0, pay_unused = 0;
        uint32_t hist[18] = {0};
        dxvk_status first_err = DXVK_OK;
        uint32_t first_err_frame = 0;
        dxvk_frame_layout l;
        int opcodes_seen = 0;
        char fcc[5];
        if (strcmp(argv[a], "--threads") == 0) {
            a++;
            continue;
        }
        if (strcmp(argv[a], "--histogram") == 0)
            continue;
        files++;
        if (!open_or_report(argv[a], &mov)) {
            files_bad++;
            continue;
        }
        in = dxvk_mov_get_info(mov);
        memset(&l, 0, sizeof l);
        for (uint32_t i = 0; i < in->frame_count; i++) {
            dxvk_consumption c;
            dxvk_status st = decode_frame(mov, i, &opt, &b, &l, &c, NULL);
            packets++;
            if (st != DXVK_OK) {
                if (!err) {
                    first_err = st;
                    first_err_frame = i;
                }
                err++;
                continue;
            }
            if (c.payload_unused ||
                (c.fse_streams && (c.fse_bits_left_min != 32 || c.fse_bits_left_max != 32)) ||
                c.literals_unused != c.literals_dangling)
                notexact++;
            else if (c.opcodes_unused)
                tail++; /* exact apart from dangling opcodes and their literals (spec 7.5) */
            pay_unused += c.payload_unused;
            lit_unused += c.literals_unused;
            ops_unused += c.opcodes_unused;
            fse += c.fse_streams;
            if (c.fse_streams) {
                if (c.fse_bits_left_min < fse_min)
                    fse_min = c.fse_bits_left_min;
                if (c.fse_bits_left_max > fse_max)
                    fse_max = c.fse_bits_left_max;
            }
            for (int k = 0; k < 18; k++)
                hist[k] += c.opcode_histogram[k];
        }
        for (int k = 0; k < 18; k++)
            opcodes_seen += hist[k] != 0;
        printf("%s\n  %s %s %ux%u packets %u errors %u inexact %u dangling-tail %u | unused: payload "
               "%llu B, literals %llu B, opcodes %llu",
               argv[a], fourcc_str(in->fourcc, fcc), in->frame_count ? dxvk_variant_name(l.variant) : "-",
               in->width, in->height, in->frame_count, err, notexact, tail, pay_unused, lit_unused,
               ops_unused);
        if (fse)
            printf(" | FSE streams %u, bits left %u..%u", fse, fse_min, fse_max);
        if (l.plane_count == 2)
            printf(" | opcode values used %d/18", opcodes_seen);
        printf("\n");
        if (l.plane_count == 2 && show_histogram) {
            printf("  opcode histogram (all channels, all packets):");
            for (int k = 0; k < 18; k++)
                printf(" %d:%u", k, hist[k]);
            printf("\n");
        }
        if (err)
            printf("  first error at frame %u: %s\n", first_err_frame, dxvk_status_string(first_err));
        errors += err;
        inexact += notexact;
        tails += tail;
        if (err)
            files_bad++;
        dxvk_mov_close(mov);
    }
    printf("\nfiles %d (bad %d) | packets %llu | errors %llu | not exactly consumed %llu | exact apart "
           "from a dangling-opcode tail (spec 7.5) %llu\n",
           files, files_bad, packets, errors, inexact, tails);
    buffers_free(&b);
    pool_destroy(pl);
    return (files_bad || errors) ? 1 : 0;
}

static int cmd_bench(int argc, char **argv)
{
    int threads = 1;
    double seconds = 2.0;
    uint32_t max_frames = 60, nframes;
    dxvk_mov *mov;
    const dxvk_mov_info *in;
    uint8_t **packets;
    size_t *sizes;
    frame_buffers b;
    dxvk_frame_layout l;
    dxvk_options opt;
    dxvk_jobs jobs;
    pool *pl = NULL;
    double t0, t_blocks, t_rgba;
    unsigned long long iters;
    if (argc < 1)
        return 2;
    for (int a = 1; a + 1 < argc; a++) {
        if (strcmp(argv[a], "--threads") == 0)
            threads = atoi(argv[a + 1]);
        else if (strcmp(argv[a], "--seconds") == 0)
            seconds = atof(argv[a + 1]);
        else if (strcmp(argv[a], "--frames") == 0)
            max_frames = (uint32_t)atoi(argv[a + 1]);
    }
    if (!open_or_report(argv[0], &mov))
        return 1;
    in = dxvk_mov_get_info(mov);
    nframes = in->frame_count < max_frames ? in->frame_count : max_frames;
    memset(&b, 0, sizeof b);
    memset(&opt, 0, sizeof opt);
    if (threads > 1) {
        pl = pool_create(threads);
        jobs.run = pool_run;
        jobs.user = pl;
        opt.jobs = &jobs;
    }
    packets = (uint8_t **)calloc(nframes, sizeof *packets);
    sizes = (size_t *)calloc(nframes, sizeof *sizes);
    for (uint32_t i = 0; i < nframes; i++) {
        packets[i] = (uint8_t *)malloc(in->max_packet_size);
        dxvk_mov_read_packet(mov, i, packets[i], in->max_packet_size, &sizes[i]);
    }
    /* warm up + layout */
    if (decode_frame(mov, 0, &opt, &b, &l, NULL, NULL) != DXVK_OK || to_rgba(&l, &opt, &b) != DXVK_OK) {
        fprintf(stderr, "%s: frame 0 does not decode\n", argv[0]);
        return 1;
    }
    for (int pass = 0; pass < 2; pass++) {
        iters = 0;
        t0 = now_seconds();
        do {
            for (uint32_t i = 0; i < nframes; i++) {
                void *planes[2] = {b.plane[0], b.plane[1]};
                const void *cplanes[2] = {b.plane[0], b.plane[1]};
                dxvk_status st = dxvk_decode_blocks(in->generation, packets[i], sizes[i], in->width,
                                                    in->height, &opt, planes, b.plane_cap, &l, NULL);
                if (st == DXVK_OK && pass == 1)
                    st = dxvk_blocks_to_rgba(&l, cplanes, &opt, b.rgba, (size_t)l.display_width * 4u);
                if (st != DXVK_OK) {
                    fprintf(stderr, "frame %u: %s\n", i, dxvk_status_string(st));
                    return 1;
                }
                iters++;
            }
        } while (now_seconds() - t0 < seconds);
        if (pass == 0)
            t_blocks = (now_seconds() - t0) * 1000.0 / (double)iters;
        else
            t_rgba = (now_seconds() - t0) * 1000.0 / (double)iters;
    }
    printf("%s | %s %ux%u | threads %d | frames %u | blocks %.3f ms/frame | blocks+rgba %.3f "
           "ms/frame\n",
           argv[0], dxvk_variant_name(l.variant), in->width, in->height, threads, nframes, t_blocks,
           t_rgba);
    for (uint32_t i = 0; i < nframes; i++)
        free(packets[i]);
    free(packets);
    free(sizes);
    buffers_free(&b);
    pool_destroy(pl);
    dxvk_mov_close(mov);
    return 0;
}

/* 16 values of a BC4 block (spec 8.3), for dump-samples. */
static void bc4_values(const uint8_t *blk, uint8_t out[16])
{
    uint32_t a0 = blk[0], a1 = blk[1];
    uint64_t idx = 0;
    uint8_t pal[8];
    for (int i = 0; i < 6; i++)
        idx |= (uint64_t)blk[2 + i] << (8 * i);
    pal[0] = (uint8_t)a0;
    pal[1] = (uint8_t)a1;
    if (a0 > a1) {
        for (uint32_t i = 1; i <= 6; i++)
            pal[1 + i] = (uint8_t)(((7u - i) * a0 + i * a1) / 7u);
    } else {
        for (uint32_t i = 1; i <= 4; i++)
            pal[1 + i] = (uint8_t)(((5u - i) * a0 + i * a1) / 5u);
        pal[6] = 0;
        pal[7] = 255;
    }
    for (int i = 0; i < 16; i++, idx >>= 3)
        out[i] = pal[idx & 7u];
}

/* Writes one BC4 channel of a plane (block bytes `block_bytes`, channel at `offset`) as w x h samples. */
static void write_samples(FILE *f, const uint8_t *plane, const dxvk_frame_layout *l, int p,
                          size_t block_bytes, size_t offset, uint32_t w, uint32_t h)
{
    uint8_t *row = (uint8_t *)malloc((size_t)l->pixels_x[p] * 4u), v[16];
    for (uint32_t by = 0; by * 4u < h; by++) {
        for (uint32_t bx = 0; bx < l->blocks_x[p]; bx++) {
            bc4_values(plane + ((size_t)by * l->blocks_x[p] + bx) * block_bytes + offset, v);
            for (int y = 0; y < 4; y++)
                memcpy(row + (size_t)y * l->pixels_x[p] + bx * 4u, v + 4 * y, 4);
        }
        for (uint32_t y = 0; y < 4 && by * 4u + y < h; y++)
            fwrite(row + (size_t)y * l->pixels_x[p], 1, w, f);
    }
    free(row);
}

/* HQ only: the decoded sample planes, cropped like a planar 4:2:0 picture: Y (W x H), Co, Cg
 * (ceil(W/2) x ceil(H/2)), then alpha (W x H) for YG10. */
static int cmd_dump_samples(int argc, char **argv)
{
    dxvk_mov *mov;
    frame_buffers b;
    dxvk_frame_layout l;
    dxvk_status st;
    FILE *f;
    uint32_t w, h, cw, ch;
    if (argc < 3)
        return 2;
    memset(&b, 0, sizeof b);
    if (!open_or_report(argv[0], &mov))
        return 1;
    st = decode_frame(mov, (uint32_t)strtoul(argv[1], NULL, 10), NULL, &b, &l, NULL, NULL);
    if (st != DXVK_OK || l.plane_count != 2) {
        fprintf(stderr, "%s: %s\n", argv[0], st != DXVK_OK ? dxvk_status_string(st) : "not HQ");
        return 1;
    }
    w = l.display_width, h = l.display_height, cw = (w + 1u) / 2u, ch = (h + 1u) / 2u;
    f = fopen(argv[2], "wb");
    if (!f)
        return 1;
    write_samples(f, b.plane[0], &l, 0, l.format[0] == DXVK_BLOCKS_BC5 ? 16u : 8u, 0, w, h);
    write_samples(f, b.plane[1], &l, 1, 16u, 0, cw, ch);
    write_samples(f, b.plane[1], &l, 1, 16u, 8, cw, ch);
    if (l.format[0] == DXVK_BLOCKS_BC5)
        write_samples(f, b.plane[0], &l, 0, 16u, 8, w, h);
    fclose(f);
    printf("%s: Y %ux%u, Co + Cg %ux%u%s\n", argv[2], w, h, cw, ch,
           l.format[0] == DXVK_BLOCKS_BC5 ? ", A" : "");
    buffers_free(&b);
    dxvk_mov_close(mov);
    return 0;
}

/* ---------------------------------------------------------------------------------------------- */
/* Encoder commands                                                                               */
/* ---------------------------------------------------------------------------------------------- */

static const char *opt_value(int argc, char **argv, const char *name)
{
    for (int a = 0; a + 1 < argc; a++)
        if (strcmp(argv[a], name) == 0)
            return argv[a + 1];
    return NULL;
}

static int opt_flag(int argc, char **argv, const char *name)
{
    for (int a = 0; a < argc; a++)
        if (strcmp(argv[a], name) == 0)
            return 1;
    return 0;
}

/* Options taking a value (skipped when collecting positional arguments). */
static int takes_value(const char *s)
{
    static const char *const names[] = {"--variant", "--effort", "--size",   "--fps",
                                        "--threads", "--opcodes", "--frames", "--seconds"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (strcmp(s, names[i]) == 0)
            return 1;
    return 0;
}

static int parse_size(const char *s, uint32_t *w, uint32_t *h)
{
    char *e;
    if (!s)
        return 0;
    *w = (uint32_t)strtoul(s, &e, 10);
    if (*e != 'x')
        return 0;
    *h = (uint32_t)strtoul(e + 1, NULL, 10);
    return *w > 0 && *h > 0;
}

static int parse_variant(const char *s, dxvk_variant *v)
{
    if (!s || strcasecmp(s, "dxt1") == 0)
        *v = DXVK_VARIANT_DXT1;
    else if (strcasecmp(s, "dxt5") == 0)
        *v = DXVK_VARIANT_DXT5;
    else if (strcasecmp(s, "ycg6") == 0)
        *v = DXVK_VARIANT_YCG6;
    else if (strcasecmp(s, "yg10") == 0)
        *v = DXVK_VARIANT_YG10;
    else
        return 0;
    return 1;
}

static int parse_encode_options(int argc, char **argv, dxvk_variant *variant,
                                dxvk_encode_options *eo)
{
    const char *effort = opt_value(argc, argv, "--effort"), *ops = opt_value(argc, argv, "--opcodes");
    memset(eo, 0, sizeof *eo);
    if (!parse_variant(opt_value(argc, argv, "--variant"), variant)) {
        fprintf(stderr, "--variant: dxt1, dxt5, ycg6 or yg10\n");
        return 0;
    }
    if (effort && strcmp(effort, "best") == 0)
        eo->effort = DXVK_EFFORT_BEST;
    else if (effort && strcmp(effort, "fast") != 0) {
        fprintf(stderr, "--effort: fast or best\n");
        return 0;
    }
    if (ops && strcmp(ops, "stored") == 0)
        eo->opcodes = DXVK_OPCODES_STORED;
    else if (ops && strcmp(ops, "auto") != 0) {
        fprintf(stderr, "--opcodes: auto or stored\n");
        return 0;
    }
    if (opt_flag(argc, argv, "--premultiplied"))
        eo->flags |= DXVK_ENC_PREMULTIPLIED_INPUT;
    if (opt_flag(argc, argv, "--stored-payload"))
        eo->flags |= DXVK_ENC_STORED_PAYLOAD;
    return 1;
}

/* "30", "29.97", "30000/1001" -> timescale / frame duration. */
static int parse_fps(const char *s, uint32_t *timescale, uint32_t *duration)
{
    double f;
    const char *slash;
    if (!s) {
        *timescale = 30000;
        *duration = 1000;
        return 1;
    }
    if ((slash = strchr(s, '/')) != NULL) {
        *timescale = (uint32_t)strtoul(s, NULL, 10);
        *duration = (uint32_t)strtoul(slash + 1, NULL, 10);
        return *timescale > 0 && *duration > 0;
    }
    f = atof(s);
    if (f <= 0)
        return 0;
    if (fabs(f - 29.97) < 0.005 || fabs(f - 23.976) < 0.005 || fabs(f - 59.94) < 0.005 ||
        fabs(f - 47.952) < 0.005 || fabs(f - 119.88) < 0.005) {
        *timescale = (uint32_t)(f * 1001.0 / 1000.0 + 0.5) * 1000u;
        *duration = 1001;
    } else {
        *timescale = (uint32_t)(f * 1000.0 + 0.5);
        *duration = 1000;
    }
    return 1;
}

/* Reads exactly n bytes from f; returns 0 at a clean end of file, -1 on a partial frame. */
static int read_frame(FILE *f, uint8_t *p, size_t n)
{
    size_t got = fread(p, 1, n, f);
    if (got == n)
        return 1;
    return got == 0 ? 0 : -1;
}

static int cmd_encode(int argc, char **argv)
{
    const char *inputs[4096], *out_path = NULL;
    int ninputs = 0, threads = 1, raw = 0, rc = 1;
    uint32_t w = 0, h = 0, timescale, duration, frames = 0, max_frames = UINT32_MAX;
    dxvk_variant variant;
    dxvk_encode_options eo;
    dxvk_encoder *enc = NULL;
    dxvk_mov_writer *mw = NULL;
    dxvk_jobs jobs;
    pool *pl = NULL;
    uint8_t *rgba = NULL, *packet = NULL;
    size_t cap, n, total = 0, largest = 0;
    FILE *rf = NULL;
    double t_enc = 0, t0;
    char err[256];
    dxvk_status st;

    for (int a = 0; a < argc; a++) {
        if (takes_value(argv[a])) {
            a++;
            continue;
        }
        if (argv[a][0] == '-' && argv[a][1] == '-')
            continue;
        if (ninputs < 4096)
            inputs[ninputs++] = argv[a];
    }
    if (ninputs < 2) {
        fprintf(stderr, "encode: need at least one input and the output .mov\n");
        return 2;
    }
    out_path = inputs[--ninputs];
    if (!parse_encode_options(argc, argv, &variant, &eo) ||
        !parse_fps(opt_value(argc, argv, "--fps"), &timescale, &duration))
        return 2;
    if (opt_value(argc, argv, "--threads"))
        threads = atoi(opt_value(argc, argv, "--threads"));
    if (opt_value(argc, argv, "--frames"))
        max_frames = (uint32_t)strtoul(opt_value(argc, argv, "--frames"), NULL, 10);
    if (threads > 1) {
        pl = pool_create(threads);
        jobs.run = pool_run;
        jobs.user = pl;
        eo.jobs = &jobs;
    }
    {
        const char *in0 = inputs[0];
        size_t l = strlen(in0);
        raw = strcmp(in0, "-") == 0 || (l > 5 && strcmp(in0 + l - 5, ".rgba") == 0) ||
              (l > 4 && strcmp(in0 + l - 4, ".raw") == 0);
    }
    if (raw) {
        if (!parse_size(opt_value(argc, argv, "--size"), &w, &h)) {
            fprintf(stderr, "encode: raw RGBA input needs --size WxH\n");
            goto done;
        }
        rf = strcmp(inputs[0], "-") == 0 ? stdin : fopen(inputs[0], "rb");
        if (!rf) {
            fprintf(stderr, "cannot open %s\n", inputs[0]);
            goto done;
        }
        rgba = (uint8_t *)malloc((size_t)w * h * 4u);
    } else {
        rgba = image_load(inputs[0], &w, &h, err, sizeof err);
        if (!rgba) {
            fprintf(stderr, "%s\n", err);
            goto done;
        }
    }
    if ((st = dxvk_encoder_create(variant, w, h, &eo, &enc)) != DXVK_OK ||
        (st = dxvk_mov_writer_open_file(out_path, w, h, timescale, duration, &mw)) != DXVK_OK) {
        fprintf(stderr, "encode: %s\n", dxvk_status_string(st));
        goto done;
    }
    cap = dxvk_encode_bound(variant, w, h);
    packet = (uint8_t *)malloc(cap);
    if (!rgba || !packet) {
        fprintf(stderr, "out of memory\n");
        goto done;
    }
    for (int i = 0; frames < max_frames; i++) {
        if (raw) {
            int r = read_frame(rf, rgba, (size_t)w * h * 4u);
            if (r < 0)
                fprintf(stderr, "warning: partial frame at the end of %s ignored\n", inputs[0]);
            if (r <= 0)
                break;
        } else {
            uint32_t iw, ih;
            if (i >= ninputs)
                break;
            if (i > 0) {
                free(rgba);
                rgba = image_load(inputs[i], &iw, &ih, err, sizeof err);
                if (!rgba) {
                    fprintf(stderr, "%s\n", err);
                    goto done;
                }
                if (iw != w || ih != h) {
                    fprintf(stderr, "%s: %ux%u, expected %ux%u\n", inputs[i], iw, ih, w, h);
                    goto done;
                }
            }
        }
        t0 = now_seconds();
        st = dxvk_encoder_encode(enc, rgba, (size_t)w * 4u, packet, cap, &n);
        t_enc += now_seconds() - t0;
        if (st == DXVK_OK)
            st = dxvk_mov_writer_add(mw, packet, n);
        if (st != DXVK_OK) {
            fprintf(stderr, "frame %u: %s\n", frames, dxvk_status_string(st));
            goto done;
        }
        total += n;
        if (n > largest)
            largest = n;
        frames++;
    }
    if (frames == 0 || (st = dxvk_mov_writer_finish(mw)) != DXVK_OK) {
        fprintf(stderr, "encode: %s\n", frames ? dxvk_status_string(st) : "no frames");
        goto done;
    }
    printf("%s: %s %ux%u, %u frames, %.4g fps (%u/%u), effort %s, opcodes %s | packets %zu bytes "
           "(mean %.0f, max %zu) | encode %.2f ms/frame (%d thread%s)\n",
           out_path, dxvk_variant_name(variant), w, h, frames, (double)timescale / duration,
           timescale, duration, eo.effort == DXVK_EFFORT_BEST ? "best" : "fast",
           eo.opcodes == DXVK_OPCODES_STORED ? "stored" : "auto", total, (double)total / frames,
           largest, t_enc * 1000.0 / frames, threads, threads == 1 ? "" : "s");
    rc = 0;
done:
    if (rf && rf != stdin)
        fclose(rf);
    dxvk_mov_writer_close(mw);
    dxvk_encoder_destroy(enc);
    free(rgba);
    free(packet);
    pool_destroy(pl);
    return rc;
}

static int cmd_bench_encode(int argc, char **argv)
{
    uint32_t w, h, nframes = 0, max_frames = 30;
    int threads = 1;
    double seconds = 3.0, t0, el;
    dxvk_variant variant;
    dxvk_encode_options eo;
    dxvk_encoder *enc;
    dxvk_jobs jobs;
    pool *pl = NULL;
    uint8_t *frames, *packet;
    size_t fbytes, cap, n, bytes = 0;
    unsigned long long iters = 0;
    FILE *f;
    if (argc < 1 || !parse_size(opt_value(argc, argv, "--size"), &w, &h) ||
        !parse_encode_options(argc, argv, &variant, &eo))
        return 2;
    if (opt_value(argc, argv, "--threads"))
        threads = atoi(opt_value(argc, argv, "--threads"));
    if (opt_value(argc, argv, "--frames"))
        max_frames = (uint32_t)strtoul(opt_value(argc, argv, "--frames"), NULL, 10);
    if (opt_value(argc, argv, "--seconds"))
        seconds = atof(opt_value(argc, argv, "--seconds"));
    if (threads > 1) {
        pl = pool_create(threads);
        jobs.run = pool_run;
        jobs.user = pl;
        eo.jobs = &jobs;
    }
    fbytes = (size_t)w * h * 4u;
    frames = (uint8_t *)malloc(fbytes * max_frames);
    f = fopen(argv[0], "rb");
    if (!f || !frames) {
        fprintf(stderr, "cannot read %s\n", argv[0]);
        return 1;
    }
    while (nframes < max_frames && read_frame(f, frames + fbytes * nframes, fbytes) == 1)
        nframes++;
    fclose(f);
    if (nframes == 0 || dxvk_encoder_create(variant, w, h, &eo, &enc) != DXVK_OK)
        return 1;
    cap = dxvk_encode_bound(variant, w, h);
    packet = (uint8_t *)malloc(cap);
    dxvk_encoder_encode(enc, frames, (size_t)w * 4u, packet, cap, &n); /* warm up */
    t0 = now_seconds();
    do {
        for (uint32_t i = 0; i < nframes; i++) {
            if (dxvk_encoder_encode(enc, frames + fbytes * i, (size_t)w * 4u, packet, cap, &n) !=
                DXVK_OK)
                return 1;
            bytes += n;
            iters++;
        }
    } while (now_seconds() - t0 < seconds);
    el = now_seconds() - t0;
    printf("%s %ux%u effort %s threads %d frames %u: %.2f ms/frame, %.1f fps, mean packet %.0f B\n",
           dxvk_variant_name(variant), w, h, eo.effort == DXVK_EFFORT_BEST ? "best" : "fast",
           threads, nframes, el * 1000.0 / (double)iters, (double)iters / el,
           (double)bytes / (double)iters);
    dxvk_encoder_destroy(enc);
    free(frames);
    free(packet);
    pool_destroy(pl);
    return 0;
}

static double psnr_of(double sse, double count)
{
    if (sse <= 0)
        return 99.0;
    return 10.0 * log10(255.0 * 255.0 * count / sse);
}

static int cmd_psnr(int argc, char **argv)
{
    dxvk_mov *mov;
    const dxvk_mov_info *in;
    frame_buffers b;
    dxvk_frame_layout l;
    dxvk_options opt;
    uint8_t *src;
    size_t fbytes;
    uint32_t max_frames = UINT32_MAX;
    double sum_rgb = 0, sum_a = 0;
    int frames = 0, alpha = 0;
    FILE *f;
    if (argc < 2)
        return 2;
    if (opt_value(argc, argv, "--frames"))
        max_frames = (uint32_t)strtoul(opt_value(argc, argv, "--frames"), NULL, 10);
    if (!open_or_report(argv[0], &mov))
        return 1;
    in = dxvk_mov_get_info(mov);
    fbytes = (size_t)in->width * in->height * 4u;
    src = (uint8_t *)malloc(fbytes);
    f = fopen(argv[1], "rb");
    if (!f || !src) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    memset(&b, 0, sizeof b);
    memset(&opt, 0, sizeof opt);
    for (uint32_t i = 0; i < in->frame_count && i < max_frames; i++) {
        double sse[4] = {0, 0, 0, 0};
        int maxe[4] = {0, 0, 0, 0};
        dxvk_status st;
        if (read_frame(f, src, fbytes) != 1)
            break;
        st = decode_frame(mov, i, &opt, &b, &l, NULL, NULL);
        if (st == DXVK_OK)
            st = to_rgba(&l, &opt, &b);
        if (st != DXVK_OK) {
            fprintf(stderr, "frame %u: %s\n", i, dxvk_status_string(st));
            return 1;
        }
        alpha = dxvk_variant_has_alpha(l.variant);
        for (size_t k = 0; k < fbytes; k++) {
            int d = (int)b.rgba[k] - (int)src[k];
            if (d < 0)
                d = -d;
            sse[k & 3u] += (double)d * d;
            if (d > maxe[k & 3u])
                maxe[k & 3u] = d;
        }
        {
            double px = (double)in->width * in->height;
            double p_rgb = psnr_of(sse[0] + sse[1] + sse[2], 3.0 * px), p_a = psnr_of(sse[3], px);
            printf("frame %u: PSNR RGB %.3f dB (R %.2f G %.2f B %.2f) max err %d/%d/%d", i, p_rgb,
                   psnr_of(sse[0], px), psnr_of(sse[1], px), psnr_of(sse[2], px), maxe[0], maxe[1],
                   maxe[2]);
            if (alpha)
                printf(" | A %.3f dB max err %d", p_a, maxe[3]);
            printf("\n");
            sum_rgb += p_rgb;
            sum_a += p_a;
        }
        frames++;
    }
    if (frames)
        printf("%s: %s %d frames, mean PSNR RGB %.3f dB%s", argv[0], dxvk_variant_name(l.variant),
               frames, sum_rgb / frames, alpha ? "" : "\n");
    if (frames && alpha)
        printf(", alpha %.3f dB\n", sum_a / frames);
    fclose(f);
    free(src);
    buffers_free(&b);
    dxvk_mov_close(mov);
    return frames ? 0 : 1;
}

static int cmd_cmp_rgba(int argc, char **argv)
{
    uint32_t w, h;
    size_t n, hist[4] = {0, 0, 0, 0}, differ = 0;
    int maxd[4] = {0, 0, 0, 0};
    uint8_t *a, *b;
    FILE *fa, *fb;
    if (argc < 3 || !parse_size(opt_value(argc, argv, "--size"), &w, &h))
        return 2;
    n = (size_t)w * h * 4u;
    a = (uint8_t *)malloc(n);
    b = (uint8_t *)malloc(n);
    fa = fopen(argv[0], "rb");
    fb = fopen(argv[1], "rb");
    if (!fa || !fb || read_frame(fa, a, n) != 1 || read_frame(fb, b, n) != 1) {
        fprintf(stderr, "cannot read two %ux%u RGBA frames\n", w, h);
        return 1;
    }
    if (opt_flag(argc, argv, "--unpremultiply-b")) {
        /* B's colour divided by its alpha (floor(c x 255 / a), clamped; alpha 0 and 255 untouched):
         * the conversion FFmpeg's DXT5 output was measured to apply (black box, QUESTIONS 11) */
        for (size_t k = 0; k < n; k += 4) {
            unsigned al = b[k + 3];
            if (al == 0 || al == 255)
                continue;
            for (int c = 0; c < 3; c++) {
                unsigned v = b[k + (size_t)c] * 255u / al;
                b[k + (size_t)c] = (uint8_t)(v > 255u ? 255u : v);
            }
        }
    }
    for (size_t k = 0; k < n; k++) {
        int d = (int)a[k] - (int)b[k];
        if (d < 0)
            d = -d;
        if (d) {
            differ++;
            hist[d == 1 ? 1 : d == 2 ? 2 : 3]++;
        }
        if (d > maxd[k & 3u])
            maxd[k & 3u] = d;
    }
    printf("%ux%u: %zu of %zu bytes differ (|d|=1: %zu, 2: %zu, >2: %zu) | max |d| R %d G %d B %d A %d\n",
           w, h, differ, n, hist[1], hist[2], hist[3], maxd[0], maxd[1], maxd[2], maxd[3]);
    fclose(fa);
    fclose(fb);
    free(a);
    free(b);
    return 0;
}

/* Decodes every packet of FILE and re-packs the same block streams with DXVKit's packer (the
 * blocks-in path) into OUT.mov: a like-for-like comparison of the lossless layer. */
static int cmd_repack(int argc, char **argv)
{
    dxvk_mov *mov;
    const dxvk_mov_info *in;
    dxvk_encoder *enc = NULL;
    dxvk_mov_writer *mw = NULL;
    dxvk_encode_options eo;
    dxvk_variant variant;
    frame_buffers b;
    dxvk_frame_layout l;
    uint8_t *packet = NULL;
    size_t cap, n, in_total = 0, out_total = 0, psize;
    int rc = 1, same = 1;
    dxvk_status st;
    if (argc < 2)
        return 2;
    memset(&b, 0, sizeof b);
    if (!parse_encode_options(argc, argv, &variant, &eo) || !open_or_report(argv[0], &mov))
        return 2;
    in = dxvk_mov_get_info(mov);
    for (uint32_t i = 0; i < in->frame_count; i++) {
        const void *planes[2];
        uint8_t *back[2] = {NULL, NULL};
        if ((st = decode_frame(mov, i, NULL, &b, &l, NULL, &psize)) != DXVK_OK) {
            fprintf(stderr, "frame %u: %s\n", i, dxvk_status_string(st));
            goto done;
        }
        if (!enc) {
            if ((st = dxvk_encoder_create(l.variant, in->width, in->height, &eo, &enc)) != DXVK_OK ||
                (st = dxvk_mov_writer_open_file(argv[1], in->width, in->height, in->timescale,
                                                in->frame_duration ? in->frame_duration : 1,
                                                &mw)) != DXVK_OK) {
                fprintf(stderr, "repack: %s\n", dxvk_status_string(st));
                goto done;
            }
            cap = dxvk_encode_bound(l.variant, in->width, in->height);
            packet = (uint8_t *)malloc(cap);
        }
        planes[0] = b.plane[0];
        planes[1] = b.plane[1];
        if ((st = dxvk_encoder_pack(enc, planes, packet, cap, &n)) != DXVK_OK ||
            (st = dxvk_mov_writer_add(mw, packet, n)) != DXVK_OK) {
            fprintf(stderr, "frame %u: %s\n", i, dxvk_status_string(st));
            goto done;
        }
        /* the repacked packet must decode to the same blocks */
        for (int p = 0; p < l.plane_count; p++) {
            back[p] = (uint8_t *)malloc(l.bytes[p]);
        }
        {
            void *bp[2] = {back[0], back[1]};
            dxvk_frame_layout l2;
            st = dxvk_decode_blocks(DXVK_GEN_DXV3, packet, n, in->width, in->height, NULL, bp,
                                    l.bytes, &l2, NULL);
            for (int p = 0; p < l.plane_count; p++)
                if (st != DXVK_OK || memcmp(back[p], b.plane[p], l.bytes[p]) != 0)
                    same = 0;
        }
        free(back[0]);
        free(back[1]);
        printf("frame %u: %s %zu -> %zu bytes (%+.2f %%)\n", i, dxvk_variant_name(l.variant), psize,
               n, 100.0 * ((double)n - (double)psize) / (double)psize);
        in_total += psize;
        out_total += n;
    }
    if (!mw || dxvk_mov_writer_finish(mw) != DXVK_OK)
        goto done;
    printf("%s -> %s: %zu -> %zu bytes (%+.2f %%), blocks identical after repack: %s\n", argv[0],
           argv[1], in_total, out_total,
           100.0 * ((double)out_total - (double)in_total) / (double)in_total, same ? "yes" : "NO");
    rc = same ? 0 : 1;
done:
    dxvk_mov_writer_close(mw);
    dxvk_encoder_destroy(enc);
    free(packet);
    buffers_free(&b);
    dxvk_mov_close(mov);
    return rc;
}

static void usage(void)
{
    fprintf(stderr,
            "usage:\n"
            "  dxvtool info FILE...\n"
            "  dxvtool dump-blocks FILE FRAME OUTPREFIX\n"
            "  dxvtool dump-rgba FILE FRAME OUT.rgba [--premultiply]\n"
            "  dxvtool verify VECTORS.TSV [--threads N]\n"
            "  dxvtool check FILE... [--threads N] [--histogram]\n"
            "  dxvtool bench FILE [--threads N] [--seconds S] [--frames N]\n"
            "  dxvtool encode IN... OUT.mov [--variant dxt1|dxt5|ycg6|yg10] [--effort fast|best]\n"
            "         [--size WxH (raw input)] [--fps 30|29.97|N/D] [--threads N] [--frames N]\n"
            "         [--opcodes auto|stored] [--premultiplied] [--stored-payload]\n"
            "         IN = PNG / PAM / PPM files, or one raw RGBA file / '-' (stdin) with --size\n"
            "  dxvtool bench-encode SOURCE.rgba --size WxH [--variant V] [--effort E] [--threads N]\n"
            "  dxvtool repack FILE.mov OUT.mov [--effort fast|best] [--opcodes auto|stored]\n"
            "  dxvtool psnr FILE.mov SOURCE.rgba [--frames N]\n"
            "  dxvtool cmp-rgba A.rgba B.rgba --size WxH [--unpremultiply-b]\n"
            "  dxvtool dump-samples FILE FRAME OUT   (HQ: Y, Co, Cg [, A] planes, 4:2:0 crop)\n"
            "  (--threads 0 = all cores)\n");
}

int main(int argc, char **argv)
{
    int rc = 2;
    for (int a = 2; a + 1 < argc; a++)
        if (strcmp(argv[a], "--threads") == 0 && atoi(argv[a + 1]) == 0) {
            static char buf[16];
            snprintf(buf, sizeof buf, "%d", default_threads());
            argv[a + 1] = buf;
        }
    if (argc >= 3 && strcmp(argv[1], "info") == 0)
        rc = cmd_info(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "dump-blocks") == 0)
        rc = cmd_dump(argc - 2, argv + 2, 0);
    else if (argc >= 3 && strcmp(argv[1], "dump-rgba") == 0)
        rc = cmd_dump(argc - 2, argv + 2, 1);
    else if (argc >= 3 && strcmp(argv[1], "verify") == 0)
        rc = cmd_verify(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "check") == 0)
        rc = cmd_check(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "bench") == 0)
        rc = cmd_bench(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "encode") == 0)
        rc = cmd_encode(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "bench-encode") == 0)
        rc = cmd_bench_encode(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "repack") == 0)
        rc = cmd_repack(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "psnr") == 0)
        rc = cmd_psnr(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "dump-samples") == 0)
        rc = cmd_dump_samples(argc - 2, argv + 2);
    else if (argc >= 3 && strcmp(argv[1], "cmp-rgba") == 0)
        rc = cmd_cmp_rgba(argc - 2, argv + 2);
    if (rc == 2)
        usage();
    return rc;
}
