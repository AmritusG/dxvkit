/*
 * libFuzzer harness: the encoder. Random size, options and pixels (RGBA path) or random block planes
 * (blocks-in path) -> packet -> DXVKit's decoder. Aborts unless the decoded planes equal the packed
 * ones and the packet is consumed exactly (no unused payload, literals or opcodes; FSE streams leave
 * 32 bits). Every buffer has its exact size so AddressSanitizer catches any overrun.
 *
 * Input: byte 0 = options (bits 0-1 variant, 2 effort, 3 opcodes stored, 4 stored payload, 5 jobs,
 * 6 blocks-in, 7 premultiplied flag), bytes 1-2 / 3-4 = width / height (reduced to 1..320),
 * byte 5 = run length for the pixel pattern, then the data (cycled to fill the picture or planes).
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#include "dxvkit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reverse_jobs(void *user, void (*job)(void *, int), void *arg, int count)
{
    (void)user;
    for (int i = count - 1; i >= 0; i--)
        job(arg, i);
}

static void fail(const char *what)
{
    fprintf(stderr, "fuzz_encode: %s\n", what);
    abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const dxvk_variant variants[4] = {DXVK_VARIANT_DXT1, DXVK_VARIANT_DXT5, DXVK_VARIANT_YCG6,
                                             DXVK_VARIANT_YG10};
    dxvk_jobs jobs = {reverse_jobs, NULL};
    dxvk_encode_options eo;
    dxvk_variant v;
    dxvk_encoder *enc;
    const dxvk_frame_layout *l;
    dxvk_consumption c;
    dxvk_frame_layout dl;
    uint32_t w, h, run;
    const uint8_t *src;
    size_t n, cap, psize = 0;
    uint8_t *planes[2] = {NULL, NULL}, *dec[2] = {NULL, NULL}, *packet, *rgba = NULL;
    int blocks_in, hq;
    dxvk_status st;

    if (size < 6)
        return 0;
    v = variants[data[0] & 3u];
    hq = v == DXVK_VARIANT_YCG6 || v == DXVK_VARIANT_YG10;
    memset(&eo, 0, sizeof eo);
    eo.effort = (data[0] & 4u) ? DXVK_EFFORT_BEST : DXVK_EFFORT_FAST;
    eo.opcodes = (data[0] & 8u) ? DXVK_OPCODES_STORED : DXVK_OPCODES_AUTO;
    if ((data[0] & 16u) && !hq)
        eo.flags |= DXVK_ENC_STORED_PAYLOAD;
    if (data[0] & 32u)
        eo.jobs = &jobs;
    if (data[0] & 128u)
        eo.flags |= DXVK_ENC_PREMULTIPLIED_INPUT;
    blocks_in = (data[0] & 64u) != 0;
    w = ((uint32_t)data[1] | ((uint32_t)data[2] << 8)) % 320u + 1u;
    h = ((uint32_t)data[3] | ((uint32_t)data[4] << 8)) % 320u + 1u;
    run = data[5] % 64u + 1u;
    src = data + 6;
    n = size - 6;

    if (dxvk_encoder_create(v, w, h, &eo, &enc) != DXVK_OK)
        fail("create");
    l = dxvk_encoder_layout(enc);
    for (int i = 0; i < l->plane_count; i++) {
        planes[i] = (uint8_t *)malloc(l->bytes[i]);
        dec[i] = (uint8_t *)malloc(l->bytes[i]);
    }
    cap = dxvk_encode_bound(v, w, h);
    packet = (uint8_t *)malloc(cap);

    if (blocks_in) {
        /* arbitrary block planes: runs of `run` blocks repeating the data, so repeats occur */
        for (int i = 0; i < l->plane_count; i++)
            for (size_t k = 0; k < l->bytes[i]; k++) {
                size_t blk = k / 8u, at = (blk / run) * 8u + k % 8u;
                planes[i][k] = n ? src[at % n] : 0;
            }
        {
            const void *cp[2] = {planes[0], planes[1]};
            st = dxvk_encoder_pack(enc, cp, packet, cap, &psize);
        }
    } else {
        size_t stride = (size_t)w * 4u;
        rgba = (uint8_t *)malloc(stride * h);
        for (size_t k = 0; k < stride * h; k++) {
            size_t px = k / 4u, at = (px / run) * 4u + k % 4u;
            rgba[k] = n ? src[at % n] : 0;
        }
        st = dxvk_encoder_encode(enc, rgba, stride, packet, cap, &psize);
        for (int i = 0; i < l->plane_count && st == DXVK_OK; i++)
            memcpy(planes[i], dxvk_encoder_plane(enc, i), l->bytes[i]);
    }
    if (st != DXVK_OK)
        fail(dxvk_status_string(st));
    if (psize > cap)
        fail("packet larger than the bound");
    {
        void *dp[2] = {dec[0], dec[1]};
        st = dxvk_decode_blocks(DXVK_GEN_DXV3, packet, psize, w, h, NULL, dp, l->bytes, &dl, &c);
    }
    if (st != DXVK_OK)
        fail(dxvk_status_string(st));
    for (int i = 0; i < l->plane_count; i++)
        if (memcmp(dec[i], planes[i], l->bytes[i]) != 0)
            fail("decoded planes differ");
    if (c.payload_unused || c.literals_unused || c.opcodes_unused ||
        (c.fse_streams && (c.fse_bits_left_min != 32 || c.fse_bits_left_max != 32)))
        fail("packet not consumed exactly");
    if (eo.opcodes == DXVK_OPCODES_STORED && c.fse_streams)
        fail("FSE with opcodes = stored");

    dxvk_encoder_destroy(enc);
    for (int i = 0; i < 2; i++) {
        free(planes[i]);
        free(dec[i]);
    }
    free(packet);
    free(rgba);
    return 0;
}
