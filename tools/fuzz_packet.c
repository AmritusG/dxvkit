/*
 * libFuzzer harness: packet decode + RGBA conversion.
 * Input: 5-byte prefix (byte 0 bit 0: generation; bytes 1-2 / 3-4: width / height, little-endian,
 * reduced to 1..2048) followed by the packet. Planes are allocated at their exact size so any
 * out-of-bounds write is caught by AddressSanitizer.
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#include "dxvkit.h"

#include <stdlib.h>
#include <string.h>

/* Runs jobs in reverse order: the result must not depend on job order. */
static void reverse_jobs(void *user, void (*job)(void *, int), void *arg, int count)
{
    (void)user;
    for (int i = count - 1; i >= 0; i--)
        job(arg, i);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    dxvk_generation gen;
    uint32_t w, h;
    dxvk_variant v;
    dxvk_frame_layout l;
    dxvk_jobs jobs = {reverse_jobs, NULL};
    dxvk_options opt = {0, NULL};
    void *planes[2] = {NULL, NULL};
    size_t caps[2] = {0, 0};
    uint8_t *packet;
    size_t n;
    if (size < 5)
        return 0;
    gen = (data[0] & 1) ? DXVK_GEN_LEGACY : DXVK_GEN_DXV3;
    w = ((uint32_t)data[1] | ((uint32_t)data[2] << 8)) % 2048u + 1u;
    h = ((uint32_t)data[3] | ((uint32_t)data[4] << 8)) % 2048u + 1u;
    if (data[0] & 2)
        opt.jobs = &jobs;
    if (data[0] & 4)
        opt.flags |= DXVK_OPT_PREMULTIPLY;
    /* the packet in its own exact-size allocation */
    n = size - 5;
    packet = (uint8_t *)malloc(n ? n : 1);
    memcpy(packet, data + 5, n);
    if (dxvk_packet_variant(gen, packet, n, &v, NULL) == DXVK_OK &&
        dxvk_layout(v, w, h, &l) == DXVK_OK) {
        for (int i = 0; i < l.plane_count; i++) {
            caps[i] = l.bytes[i];
            planes[i] = malloc(caps[i]);
        }
        if (dxvk_decode_blocks(gen, packet, n, w, h, &opt, planes, caps, &l, NULL) == DXVK_OK) {
            size_t stride = (size_t)w * 4u;
            uint8_t *rgba = (uint8_t *)malloc(stride * h);
            const void *cp[2] = {planes[0], planes[1]};
            dxvk_blocks_to_rgba(&l, cp, &opt, rgba, stride);
            free(rgba);
        }
        free(planes[0]);
        free(planes[1]);
    }
    free(packet);
    return 0;
}
