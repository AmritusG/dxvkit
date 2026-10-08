// DXVKit C++ wrapper smoke test: decodes every frame of every valid test-vector stream through
// dxvk::Movie / dxvk::Frame, single-threaded and with a std::thread job runner, and checks that both
// give identical block streams and RGBA.
//
// MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
#include "dxvkit.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

static void run_threads(void *, void (*job)(void *, int), void *arg, int count)
{
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++)
        threads.emplace_back(job, arg, i);
    for (auto &t : threads)
        t.join();
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: dxvkit_cpp_test TEST_VECTORS_DIR\n");
        return 2;
    }
    const std::string dir = argv[1];
    const char *files[] = {
        "resolume/resolume-alley_dxt1_testsrc2_1920x1080", "resolume/resolume-alley_dxt5_testsrc2_1920x1080",
        "resolume/resolume-alley_ycg6_testsrc2_1920x1080", "resolume/resolume-alley_yg10_testsrc2_1920x1080",
        "glenc/glenc_yg10_pat333x221",                     "glenc/glenc_ycg6_pat18x10",
        "synthetic/synthetic-dxdi_dxt1-lzf_pat100x36",     "synthetic/synthetic-dxdi_dxt5-lzf_pat64",
    };
    dxvk_jobs jobs{run_threads, nullptr};
    dxvk_options threaded{DXVK_OPT_PREMULTIPLY, &jobs};
    dxvk_options single{DXVK_OPT_PREMULTIPLY, nullptr};
    int failures = 0, frames = 0;
    for (const char *f : files) {
        std::string path = dir + "/streams/" + f + ".mov";
        dxvk::Movie movie;
        if (movie.open(path.c_str()) != DXVK_OK) {
            std::fprintf(stderr, "cannot open %s\n", path.c_str());
            return 1;
        }
        dxvk::Movie moved = std::move(movie); // move semantics keep the handle
        dxvk::Frame a, b;
        for (uint32_t i = 0; i < moved.frame_count(); i++) {
            frames++;
            if (moved.decode(i, a, &single) != DXVK_OK || moved.decode(i, b, &threaded) != DXVK_OK) {
                std::fprintf(stderr, "%s frame %u: decode failed\n", f, i);
                failures++;
                continue;
            }
            const auto &l = a.layout();
            std::vector<uint8_t> ra(size_t(l.display_width) * l.display_height * 4);
            std::vector<uint8_t> rb(ra.size());
            bool same = a.to_rgba(ra.data(), l.display_width * 4, &single) == DXVK_OK &&
                        b.to_rgba(rb.data(), l.display_width * 4, &threaded) == DXVK_OK && ra == rb;
            for (int p = 0; p < a.plane_count(); p++)
                same = same && std::memcmp(a.plane(p), b.plane(p), a.plane_bytes(p)) == 0;
            if (!same) {
                std::fprintf(stderr, "%s frame %u: threaded output differs\n", f, i);
                failures++;
            }
        }
    }
    std::printf("cpp wrapper: %d frames, %d failures\n", frames, failures);
    return failures ? 1 : 0;
}
