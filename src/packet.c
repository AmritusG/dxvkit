/*
 * DXVKit: packet headers, frame geometry and dispatch to the lossless layers.
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#include "dxvk_priv.h"

/* DXV3 texture tags: the four header bytes read as a little-endian u32 (spec 3.1). */
#define TAG_DXT1 0x44585431u
#define TAG_DXT5 0x44585435u
#define TAG_YCG6 0x59434736u
#define TAG_YG10 0x59473130u

#define DXV3_HEADER 12u
#define LEGACY_HEADER 4u

const char *dxvk_status_string(dxvk_status status)
{
    switch (status) {
    case DXVK_OK: return "ok";
    case DXVK_ERR_ARGUMENT: return "invalid argument";
    case DXVK_ERR_BUFFER_TOO_SMALL: return "output buffer too small";
    case DXVK_ERR_TRUNCATED: return "packet shorter than its header or its declared payload";
    case DXVK_ERR_UNKNOWN_VARIANT: return "unknown texture variant";
    case DXVK_ERR_UNSUPPORTED: return "stored flag on a High Quality variant";
    case DXVK_ERR_PAYLOAD_END: return "payload ends before the texture is complete";
    case DXVK_ERR_BAD_DISTANCE: return "copy distance reaches before the first block";
    case DXVK_ERR_LZF: return "malformed LZF stream";
    case DXVK_ERR_LZF_LENGTH: return "LZF output length differs from the texture size";
    case DXVK_ERR_HQ_SECTION: return "HQ section header out of range";
    case DXVK_ERR_HQ_OPCODE_COUNT: return "HQ opcode count exceeds the block count";
    case DXVK_ERR_FSE_TABLE: return "malformed FSE frequency table";
    case DXVK_ERR_FSE_STREAM: return "malformed FSE bitstream";
    case DXVK_ERR_HQ_OPCODES_USED_UP: return "HQ opcodes used up before the texture is complete";
    case DXVK_ERR_HQ_BAD_OPCODE: return "HQ opcode value out of range";
    case DXVK_ERR_HQ_EMPTY_SLOT: return "HQ dictionary slot is empty";
    case DXVK_ERR_HQ_LITERALS: return "HQ literal read past the literal area";
    case DXVK_ERR_IO: return "I/O error";
    case DXVK_ERR_NOT_MOV: return "not a QuickTime file";
    case DXVK_ERR_NO_DXV_TRACK: return "no DXV video track";
    case DXVK_ERR_MOV_CORRUPT: return "malformed QuickTime file";
    case DXVK_ERR_OUT_OF_MEMORY: return "out of memory";
    case DXVK_ERR_RANGE: return "frame index out of range";
    }
    return "unknown status";
}

const char *dxvk_variant_name(dxvk_variant variant)
{
    switch (variant) {
    case DXVK_VARIANT_DXT1: return "DXT1";
    case DXVK_VARIANT_DXT5: return "DXT5";
    case DXVK_VARIANT_YCG6: return "YCG6";
    case DXVK_VARIANT_YG10: return "YG10";
    case DXVK_VARIANT_LEGACY_DXT1: return "legacy DXT1";
    case DXVK_VARIANT_LEGACY_DXT5: return "legacy DXT5";
    }
    return "unknown";
}

int dxvk_variant_has_alpha(dxvk_variant variant)
{
    return variant == DXVK_VARIANT_DXT5 || variant == DXVK_VARIANT_YG10 ||
           variant == DXVK_VARIANT_LEGACY_DXT5;
}

dxvk_status dxvk_layout(dxvk_variant variant, uint32_t width, uint32_t height,
                        dxvk_frame_layout *layout)
{
    dxvk_frame_layout l;
    if (!layout || width == 0 || height == 0 || width > DXVK_MAX_DIMENSION ||
        height > DXVK_MAX_DIMENSION)
        return DXVK_ERR_ARGUMENT;
    memset(&l, 0, sizeof l);
    l.variant = variant;
    l.display_width = width;
    l.display_height = height;
    l.coded_width = (width + 15u) & ~15u;
    l.coded_height = (height + 15u) & ~15u;
    l.pixels_x[0] = l.coded_width;
    l.pixels_y[0] = l.coded_height;
    l.blocks_x[0] = l.coded_width / 4u;
    l.blocks_y[0] = l.coded_height / 4u;
    l.plane_count = 1;
    switch (variant) {
    case DXVK_VARIANT_DXT1:
    case DXVK_VARIANT_LEGACY_DXT1: l.format[0] = DXVK_BLOCKS_BC1; break;
    case DXVK_VARIANT_DXT5:
    case DXVK_VARIANT_LEGACY_DXT5: l.format[0] = DXVK_BLOCKS_BC3; break;
    case DXVK_VARIANT_YCG6: l.format[0] = DXVK_BLOCKS_BC4; break;
    case DXVK_VARIANT_YG10: l.format[0] = DXVK_BLOCKS_BC5; break;
    default: return DXVK_ERR_ARGUMENT;
    }
    if (variant == DXVK_VARIANT_YCG6 || variant == DXVK_VARIANT_YG10) {
        l.plane_count = 2;
        l.format[1] = DXVK_BLOCKS_BC5;
        l.pixels_x[1] = l.coded_width / 2u;
        l.pixels_y[1] = l.coded_height / 2u;
        l.blocks_x[1] = l.coded_width / 8u;
        l.blocks_y[1] = l.coded_height / 8u;
    }
    for (int i = 0; i < l.plane_count; i++) {
        size_t block_bytes =
            (l.format[i] == DXVK_BLOCKS_BC1 || l.format[i] == DXVK_BLOCKS_BC4) ? 8u : 16u;
        l.bytes[i] = (size_t)l.blocks_x[i] * l.blocks_y[i] * block_bytes;
    }
    *layout = l;
    return DXVK_OK;
}

/* Parses the header. *header_size = header bytes, *payload_size = declared payload length. */
static dxvk_status parse_header(dxvk_generation generation, const uint8_t *p, size_t size,
                                dxvk_variant *variant, int *stored, size_t *header_size,
                                size_t *payload_size)
{
    if (generation == DXVK_GEN_DXV3) {
        uint32_t tag;
        if (size < DXV3_HEADER)
            return DXVK_ERR_TRUNCATED;
        tag = dxvk_le32(p);
        switch (tag) {
        case TAG_DXT1: *variant = DXVK_VARIANT_DXT1; break;
        case TAG_DXT5: *variant = DXVK_VARIANT_DXT5; break;
        case TAG_YCG6: *variant = DXVK_VARIANT_YCG6; break;
        case TAG_YG10: *variant = DXVK_VARIANT_YG10; break;
        default: return DXVK_ERR_UNKNOWN_VARIANT;
        }
        *stored = p[6] != 0;
        *header_size = DXV3_HEADER;
        *payload_size = dxvk_le32(p + 8);
    } else if (generation == DXVK_GEN_LEGACY) {
        uint32_t word, type, nibble;
        if (size < LEGACY_HEADER)
            return DXVK_ERR_TRUNCATED;
        word = dxvk_le32(p);
        type = word >> 24;
        nibble = type & 0x0Fu;
        if (type & 0x40u)
            *variant = DXVK_VARIANT_LEGACY_DXT5;
        else if ((type & 0x20u) || nibble == 2u)
            *variant = DXVK_VARIANT_LEGACY_DXT1;
        else
            return DXVK_ERR_UNKNOWN_VARIANT;
        *stored = (type & 0x80u) != 0;
        *header_size = LEGACY_HEADER;
        *payload_size = word & 0xFFFFFFu;
    } else {
        return DXVK_ERR_ARGUMENT;
    }
    if (*payload_size > size - *header_size)
        return DXVK_ERR_TRUNCATED;
    return DXVK_OK;
}

dxvk_status dxvk_packet_variant(dxvk_generation generation, const void *packet, size_t packet_size,
                                dxvk_variant *variant, int *stored)
{
    size_t header_size, payload_size;
    dxvk_variant v;
    int s;
    dxvk_status st;
    if (!packet)
        return DXVK_ERR_ARGUMENT;
    st = parse_header(generation, (const uint8_t *)packet, packet_size, &v, &s, &header_size,
                      &payload_size);
    if (st != DXVK_OK)
        return st;
    if (variant)
        *variant = v;
    if (stored)
        *stored = s;
    return DXVK_OK;
}

dxvk_status dxvk_decode_blocks(dxvk_generation generation, const void *packet, size_t packet_size,
                               uint32_t width, uint32_t height, const dxvk_options *options,
                               void *const planes[2], const size_t plane_capacity[2],
                               dxvk_frame_layout *layout_out, dxvk_consumption *consumption)
{
    const uint8_t *p = (const uint8_t *)packet;
    const uint8_t *payload;
    size_t header_size, payload_size, used = 0;
    dxvk_variant variant;
    dxvk_frame_layout layout;
    dxvk_consumption cons;
    int stored;
    dxvk_status st;

    if (!packet || !planes || !plane_capacity)
        return DXVK_ERR_ARGUMENT;
    st = parse_header(generation, p, packet_size, &variant, &stored, &header_size, &payload_size);
    if (st != DXVK_OK)
        return st;
    st = dxvk_layout(variant, width, height, &layout);
    if (st != DXVK_OK)
        return st;
    for (int i = 0; i < layout.plane_count; i++) {
        if (!planes[i])
            return DXVK_ERR_ARGUMENT;
        if (plane_capacity[i] < layout.bytes[i])
            return DXVK_ERR_BUFFER_TOO_SMALL;
    }
    payload = p + header_size;
    memset(&cons, 0, sizeof cons);

    if (stored) {
        if (variant == DXVK_VARIANT_YCG6 || variant == DXVK_VARIANT_YG10)
            return DXVK_ERR_UNSUPPORTED;
        if (payload_size < layout.bytes[0])
            return DXVK_ERR_PAYLOAD_END;
        memcpy(planes[0], payload, layout.bytes[0]);
        used = layout.bytes[0];
    } else {
        size_t blocks = (size_t)layout.blocks_x[0] * layout.blocks_y[0];
        switch (variant) {
        case DXVK_VARIANT_LEGACY_DXT1:
        case DXVK_VARIANT_LEGACY_DXT5:
            st = dxvk_lzf_decode(payload, payload_size, (uint8_t *)planes[0], layout.bytes[0]);
            used = payload_size;
            break;
        case DXVK_VARIANT_DXT1:
            st = dxvk_decode_dxt1(payload, payload_size, (uint8_t *)planes[0], blocks, &used);
            break;
        case DXVK_VARIANT_DXT5:
            st = dxvk_decode_dxt5(payload, payload_size, (uint8_t *)planes[0], blocks, &used);
            break;
        case DXVK_VARIANT_YCG6:
        case DXVK_VARIANT_YG10:
            st = dxvk_decode_hq(payload, payload_size, &layout, (uint8_t *)planes[0],
                                (uint8_t *)planes[1], options ? options->jobs : NULL, &cons);
            used = payload_size - cons.payload_unused;
            break;
        }
        if (st != DXVK_OK)
            return st;
    }
    if (layout_out)
        *layout_out = layout;
    if (consumption) {
        cons.packet_size = packet_size;
        cons.payload_size = payload_size;
        cons.payload_unused = payload_size - used;
        *consumption = cons;
    }
    return DXVK_OK;
}
