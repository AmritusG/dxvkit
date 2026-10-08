/*
 * Writes a fuzz seed corpus from DXV files: for every packet, <outdir>/pkt_<n> = 5-byte prefix +
 * packet (fuzz_packet's input format). Usage: fuzz_corpus OUTDIR FILE...
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#include "dxvkit.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    unsigned count = 0;
    for (int a = 2; a < argc; a++) {
        dxvk_mov *mov;
        const dxvk_mov_info *in;
        uint8_t *buf;
        if (dxvk_mov_open_file(argv[a], &mov) != DXVK_OK)
            continue;
        in = dxvk_mov_get_info(mov);
        buf = (uint8_t *)malloc((size_t)in->max_packet_size + 5u);
        for (uint32_t i = 0; i < in->frame_count; i++) {
            size_t n;
            char path[4096];
            FILE *f;
            if (dxvk_mov_read_packet(mov, i, buf + 5, in->max_packet_size, &n) != DXVK_OK)
                continue;
            buf[0] = in->generation == DXVK_GEN_LEGACY ? 1 : 0;
            buf[1] = (uint8_t)((in->width - 1u) & 0xFF);
            buf[2] = (uint8_t)((in->width - 1u) >> 8);
            buf[3] = (uint8_t)((in->height - 1u) & 0xFF);
            buf[4] = (uint8_t)((in->height - 1u) >> 8);
            snprintf(path, sizeof path, "%s/pkt_%04u", argv[1], count++);
            if ((f = fopen(path, "wb")) != NULL) {
                fwrite(buf, 1, n + 5u, f);
                fclose(f);
            }
        }
        free(buf);
        dxvk_mov_close(mov);
    }
    printf("%u packets\n", count);
    return 0;
}
