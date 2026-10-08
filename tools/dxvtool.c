/*
 * dxvtool: command-line front end for DXVKit.
 *
 *   dxvtool info FILE...                        container + packet summary
 *   dxvtool dump-blocks FILE FRAME OUTPREFIX    block stream(s) of one frame
 *   dxvtool dump-rgba FILE FRAME OUT [--premultiply]
 *   dxvtool verify VECTORS.TSV [--threads N]    every test-vector row, byte-exact
 *   dxvtool check FILE... [--threads N]         decode every packet, report byte consumption
 *   dxvtool bench FILE [--threads N] [--seconds S] [--frames N]
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    int threads = 1, files = 0, files_bad = 0;
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

static void usage(void)
{
    fprintf(stderr,
            "usage:\n"
            "  dxvtool info FILE...\n"
            "  dxvtool dump-blocks FILE FRAME OUTPREFIX\n"
            "  dxvtool dump-rgba FILE FRAME OUT.rgba [--premultiply]\n"
            "  dxvtool verify VECTORS.TSV [--threads N]\n"
            "  dxvtool check FILE... [--threads N]\n"
            "  dxvtool bench FILE [--threads N] [--seconds S] [--frames N]\n"
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
    if (rc == 2)
        usage();
    return rc;
}
