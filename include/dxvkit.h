/*
 * DXVKit: a clean-room decoder for Resolume's DXV video codec.
 *
 * MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
 */
#ifndef DXVKIT_H
#define DXVKIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DXVK_VERSION_MAJOR 0
#define DXVK_VERSION_MINOR 1
#define DXVK_VERSION_PATCH 0

/* Largest display width / height accepted (pixels). */
#define DXVK_MAX_DIMENSION 16384

/* ------------------------------------------------------------------------------------------------ */
/* Status codes                                                                                     */
/* ------------------------------------------------------------------------------------------------ */

typedef enum dxvk_status {
    DXVK_OK = 0,
    DXVK_ERR_ARGUMENT,          /* NULL pointer, bad dimension, unknown option */
    DXVK_ERR_BUFFER_TOO_SMALL,  /* a caller buffer is smaller than required */
    DXVK_ERR_TRUNCATED,         /* packet shorter than its header, or header length > packet */
    DXVK_ERR_UNKNOWN_VARIANT,   /* unknown DXV3 tag, or legacy type word selects no texture */
    DXVK_ERR_UNSUPPORTED,       /* stored flag on an HQ variant */
    DXVK_ERR_PAYLOAD_END,       /* the payload ends before the texture is complete */
    DXVK_ERR_BAD_DISTANCE,      /* a copy reaches before the first block */
    DXVK_ERR_LZF,               /* malformed LZF stream (truncated op, reference before start) */
    DXVK_ERR_LZF_LENGTH,        /* LZF output length differs from the texture size */
    DXVK_ERR_HQ_SECTION,        /* HQ section header: opcode-area offset out of range */
    DXVK_ERR_HQ_OPCODE_COUNT,   /* HQ opcode count larger than the channel's block count */
    DXVK_ERR_FSE_TABLE,         /* FSE frequency table malformed or truncated */
    DXVK_ERR_FSE_STREAM,        /* FSE bitstream area too short, past the payload, or exhausted */
    DXVK_ERR_HQ_OPCODES_USED_UP,/* a block needs an opcode after the channel's opcodes are used up */
    DXVK_ERR_HQ_BAD_OPCODE,     /* opcode value 18 or larger */
    DXVK_ERR_HQ_EMPTY_SLOT,     /* dictionary slot used before anything was stored in it */
    DXVK_ERR_HQ_LITERALS,       /* literal read past the literal area */
    DXVK_ERR_IO,                /* file could not be opened or read */
    DXVK_ERR_NOT_MOV,           /* not a QuickTime file */
    DXVK_ERR_NO_DXV_TRACK,      /* QuickTime file without a DXV video track */
    DXVK_ERR_MOV_CORRUPT,       /* malformed or inconsistent QuickTime structures */
    DXVK_ERR_OUT_OF_MEMORY,
    DXVK_ERR_RANGE              /* frame index out of range */
} dxvk_status;

/* Static English description of a status code. Never NULL. */
const char *dxvk_status_string(dxvk_status status);

/* ------------------------------------------------------------------------------------------------ */
/* Variants and geometry                                                                            */
/* ------------------------------------------------------------------------------------------------ */

typedef enum dxvk_generation {
    DXVK_GEN_DXV3 = 0,   /* sample-entry FourCC 'DXD3' */
    DXVK_GEN_LEGACY = 1  /* sample-entry FourCC 'DXDI' (DXV1 / DXV2) */
} dxvk_generation;

typedef enum dxvk_variant {
    DXVK_VARIANT_DXT1 = 0,         /* DXV3 Normal Quality: BC1 */
    DXVK_VARIANT_DXT5,             /* DXV3 Normal Quality With Alpha: BC3 */
    DXVK_VARIANT_YCG6,             /* DXV3 High Quality: Y BC4 + CoCg BC5 */
    DXVK_VARIANT_YG10,             /* DXV3 High Quality With Alpha: YA BC5 + CoCg BC5 */
    DXVK_VARIANT_LEGACY_DXT1,      /* DXDI: BC1 */
    DXVK_VARIANT_LEGACY_DXT5       /* DXDI: BC3 */
} dxvk_variant;

/* Name such as "DXT1", "YCG6", "legacy DXT1". Never NULL. */
const char *dxvk_variant_name(dxvk_variant variant);

/* 1 when the variant carries alpha (DXT5, YG10, legacy DXT5), else 0. */
int dxvk_variant_has_alpha(dxvk_variant variant);

/* Block-stream layout per plane (the GPU upload format). */
typedef enum dxvk_block_format {
    DXVK_BLOCKS_NONE = 0,
    DXVK_BLOCKS_BC1,   /* 8-byte blocks, opaque (upload as an opaque BC1 format) */
    DXVK_BLOCKS_BC3,   /* 16-byte blocks */
    DXVK_BLOCKS_BC4,   /* 8-byte blocks, unsigned, one channel */
    DXVK_BLOCKS_BC5    /* 16-byte blocks: channel 0 BC4 block then channel 1 BC4 block, unsigned */
} dxvk_block_format;

/*
 * Geometry of a frame. The coded texture is the display size rounded up to 16 on both axes; all block
 * streams cover the whole coded texture, row by row, top row first.
 *
 *   plane 0: DXT1 / legacy DXT1: BC1 colour.        DXT5 / legacy DXT5: BC3 colour + alpha.
 *            YCG6: BC4 luma (Y).                    YG10: BC5 layout, channel 0 = Y, channel 1 = alpha.
 *            Size: coded_width x coded_height pixels.
 *   plane 1: YCG6 / YG10 only: BC5 layout, channel 0 = Co, channel 1 = Cg (YCoCg chroma, +128 bias).
 *            Size: coded_width/2 x coded_height/2 samples. Other variants: none (size 0).
 */
typedef struct dxvk_frame_layout {
    dxvk_variant variant;
    uint32_t display_width, display_height;
    uint32_t coded_width, coded_height;
    int plane_count;                       /* 1 or 2 */
    dxvk_block_format format[2];
    uint32_t blocks_x[2], blocks_y[2];     /* blocks per row / block rows */
    uint32_t pixels_x[2], pixels_y[2];     /* plane size in pixels (samples) */
    size_t bytes[2];                       /* exact block-stream size of each plane */
} dxvk_frame_layout;

/* Fills *layout for a variant and display size (1..DXVK_MAX_DIMENSION). */
dxvk_status dxvk_layout(dxvk_variant variant, uint32_t width, uint32_t height,
                        dxvk_frame_layout *layout);

/*
 * Reads the packet header only: the variant and whether the payload is stored. Cheap; does not
 * validate the payload. For DXV3 the packet's own tag decides; for legacy the type word decides.
 */
dxvk_status dxvk_packet_variant(dxvk_generation generation, const void *packet, size_t packet_size,
                                dxvk_variant *variant, int *stored);

/* ------------------------------------------------------------------------------------------------ */
/* Packet decoding                                                                                  */
/* ------------------------------------------------------------------------------------------------ */

/*
 * Optional parallelism. run() must call job(job_arg, i) once for every i in 0..count-1, in any order,
 * possibly concurrently, and return when all calls have returned. The core never creates threads.
 * With no runner (NULL) everything runs on the calling thread.
 */
typedef struct dxvk_jobs {
    void (*run)(void *user, void (*job)(void *job_arg, int index), void *job_arg, int count);
    void *user;
} dxvk_jobs;

/* Decode options. A zeroed struct (or NULL) = defaults. */
#define DXVK_OPT_PREMULTIPLY 0x1u  /* RGBA output: convert straight alpha to premultiplied */

typedef struct dxvk_options {
    uint32_t flags;          /* DXVK_OPT_* */
    const dxvk_jobs *jobs;   /* NULL = single-threaded */
} dxvk_options;

/*
 * How much of the packet a decode used. Filled on success when requested. A packet "consumes its bytes
 * exactly" when payload_unused == 0, literals_unused == 0, opcodes_unused == 0 and fse_bits_left is 32
 * for every FSE stream (Resolume's encoders; see the spec). Resolume's HQ encoder sometimes leaves one
 * opcode after the texture is complete (spec 7.5); its literal bytes are then unread too:
 * literals_unused == literals_dangling means nothing else was left.
 */
typedef struct dxvk_consumption {
    size_t packet_size;
    size_t payload_size;      /* bytes after the header */
    size_t payload_unused;    /* payload bytes after the last byte the decoder needed */
    size_t literals_unused;   /* HQ: literal-area bytes not read (both sections) */
    size_t literals_dangling; /* HQ: literal bytes the unused opcodes would have read */
    uint32_t opcodes_unused;  /* HQ: opcodes decoded but not needed (all channels) */
    uint32_t fse_streams;     /* HQ: number of FSE-coded opcode streams */
    uint32_t fse_bits_left_min, fse_bits_left_max; /* HQ: bits left unread per FSE stream */
    uint32_t opcode_histogram[18]; /* HQ: opcodes used, by value */
} dxvk_consumption;

/*
 * Decodes one packet into block streams (output A). planes[i] must point to at least
 * layout.bytes[i] bytes for every plane of the variant (plane_count); capacities in plane_capacity.
 * width / height = display size from the container. The variant is taken from the packet; on success
 * *layout (optional) describes what was written. consumption may be NULL.
 *
 * Thread safety: no shared state; concurrent calls on different buffers are fine.
 */
dxvk_status dxvk_decode_blocks(dxvk_generation generation, const void *packet, size_t packet_size,
                               uint32_t width, uint32_t height, const dxvk_options *options,
                               void *const planes[2], const size_t plane_capacity[2],
                               dxvk_frame_layout *layout, dxvk_consumption *consumption);

/*
 * Converts decoded block streams (output A, layout as returned by dxvk_decode_blocks) to RGBA8 at the
 * display size (output B): rgba points to the top-left pixel, stride = bytes per row (>= width x 4).
 * DXT1 variants are opaque (alpha 255). HQ chroma is sampled nearest-neighbour. Alpha is left as
 * decoded unless options->flags has DXVK_OPT_PREMULTIPLY.
 */
dxvk_status dxvk_blocks_to_rgba(const dxvk_frame_layout *layout, const void *const planes[2],
                                const dxvk_options *options, uint8_t *rgba, size_t stride);

/* ------------------------------------------------------------------------------------------------ */
/* QuickTime demuxer                                                                                */
/* ------------------------------------------------------------------------------------------------ */

typedef struct dxvk_mov dxvk_mov;

/* Random-access reader: read_at returns the number of bytes read (fewer only at end of data / error). */
typedef struct dxvk_reader {
    size_t (*read_at)(void *user, uint64_t offset, void *buffer, size_t size);
    uint64_t size;   /* total size of the data in bytes */
    void *user;
    void (*close)(void *user);  /* optional; called by dxvk_mov_close */
} dxvk_reader;

typedef struct dxvk_mov_info {
    dxvk_generation generation;
    uint32_t fourcc;               /* sample-entry data format, big-endian value ('DXD3' = 0x44584433) */
    uint32_t width, height;        /* display size */
    uint32_t frame_count;
    uint32_t timescale;            /* media timescale (units per second) */
    uint32_t frame_duration;       /* first stts delta, in timescale units (0 if none) */
    double frame_rate;             /* timescale / frame_duration, 0 if unknown */
    uint64_t duration;             /* media duration, timescale units */
    uint32_t max_packet_size;      /* largest sample, bytes */
    uint32_t track_id;
} dxvk_mov_info;

/* Opens a file by path (stdio). The object reads the file on demand; close it with dxvk_mov_close. */
dxvk_status dxvk_mov_open_file(const char *path, dxvk_mov **mov);
/* Opens an in-memory file. The memory must outlive the object. */
dxvk_status dxvk_mov_open_memory(const void *data, size_t size, dxvk_mov **mov);
/* Opens through a caller reader (copied; its close() runs on dxvk_mov_close, also on failure). */
dxvk_status dxvk_mov_open_reader(const dxvk_reader *reader, dxvk_mov **mov);
void dxvk_mov_close(dxvk_mov *mov);

const dxvk_mov_info *dxvk_mov_get_info(const dxvk_mov *mov);
/* Byte offset and size of a sample. */
dxvk_status dxvk_mov_packet_location(const dxvk_mov *mov, uint32_t index, uint64_t *offset,
                                     uint32_t *size);
/* Reads sample `index` into buffer (capacity bytes); *size = the packet size. One thread at a time per
 * dxvk_mov object. */
dxvk_status dxvk_mov_read_packet(dxvk_mov *mov, uint32_t index, void *buffer, size_t capacity,
                                 size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* DXVKIT_H */
