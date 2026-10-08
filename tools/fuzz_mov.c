/*
 * libFuzzer harness: QuickTime demuxer (+ decode of up to 8 packets of small files).
 * Input: a whole .mov file.
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#include "dxvkit.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    dxvk_mov *mov;
    const dxvk_mov_info *in;
    uint8_t *packet;
    if (dxvk_mov_open_memory(data, size, &mov) != DXVK_OK)
        return 0;
    in = dxvk_mov_get_info(mov);
    packet = (uint8_t *)malloc(in->max_packet_size ? in->max_packet_size : 1);
    for (uint32_t i = 0; i < in->frame_count && i < 8; i++) {
        size_t n;
        uint64_t off;
        uint32_t sz;
        if (dxvk_mov_packet_location(mov, i, &off, &sz) != DXVK_OK || off + sz > size)
            abort(); /* the demuxer promised every sample lies inside the file */
        if (dxvk_mov_read_packet(mov, i, packet, in->max_packet_size, &n) != DXVK_OK)
            abort();
        if (in->width <= 512 && in->height <= 512) {
            dxvk_frame_layout big;
            if (dxvk_layout(DXVK_VARIANT_YG10, in->width, in->height, &big) == DXVK_OK) {
                void *planes[2] = {malloc(big.bytes[0]), malloc(big.bytes[1])};
                dxvk_decode_blocks(in->generation, packet, n, in->width, in->height, NULL, planes,
                                   big.bytes, NULL, NULL);
                free(planes[0]);
                free(planes[1]);
            }
        }
    }
    free(packet);
    dxvk_mov_close(mov);
    return 0;
}
