// Draws a fixed pseudo-random set of large GS primitives and prints a hash of GS memory.
// Run with PS2X_GS_THREADS=1 and with more threads: the hashes must match.
#include "runtime/gs/gs_frontend.h"
#include "runtime/gs/gs_types.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return (uint32_t)(g_rng >> 16); }

int main()
{
    std::vector<uint8_t> vram(4u * 1024u * 1024u);
    for (auto &b : vram) b = (uint8_t)rnd();
    GS gs;
    gs.init(vram.data(), (uint32_t)vram.size(), nullptr);
    gs.writeRegister(GS_REG_FRAME_1, (10ull << 16));                       // FBP 0, 640 wide, CT32
    gs.writeRegister(GS_REG_ZBUF_1, 140ull);                                // ZBP page 140 (after the frame), Z32
    gs.writeRegister(GS_REG_SCISSOR_1, (639ull << 16) | (447ull << 48));
    gs.writeRegister(GS_REG_XYOFFSET_1, 0ull);
    const uint32_t psms[] = {GS_PSM_CT32, GS_PSM_CT16, GS_PSM_T8, GS_PSM_T4};
    auto start = std::chrono::steady_clock::now();
    for (int n = 0; n < 400; ++n)
    {
        const uint64_t test = (1ull << 16) | ((uint64_t)(2 + rnd() % 2) << 17) |   // Z test GEQUAL/GREATER
                              ((rnd() % 3 == 0) ? (1ull | (5ull << 1) | ((uint64_t)(rnd() & 0xFF) << 4)) : 0ull);
        gs.writeRegister(GS_REG_TEST_1, test);
        gs.writeRegister(GS_REG_ALPHA_1, (uint64_t)(rnd() & 0xFF) | ((uint64_t)(rnd() & 0xFF) << 32));
        const uint64_t psm = psms[rnd() % 4];
        const uint64_t tex0 = (uint64_t)(9000 + (rnd() % 2000)) | (4ull << 14) | (psm << 20) | (8ull << 26) | (8ull << 30) |
                              (1ull << 34) | ((uint64_t)(rnd() % 4) << 35) | ((uint64_t)(13000 + rnd() % 400) << 37) |
                              (0ull << 51) | (1ull << 61);
        gs.writeRegister(GS_REG_TEX0_1, tex0);
        gs.writeRegister(GS_REG_TEX1_1, (rnd() % 2) ? (1ull << 5) | (1ull << 6) : 0ull);
        if (rnd() % 5 == 0)
            gs.writeRegister(GS_REG_TEXFLUSH, 0ull);
        const bool sprite = rnd() % 2;
        const uint64_t prim = (sprite ? GS_PRIM_SPRITE : GS_PRIM_TRIANGLE) | (1ull << 3) /*IIP*/ |
                              ((rnd() % 3) ? (1ull << 4) : 0ull) /*TME*/ | ((rnd() % 2) ? (1ull << 6) : 0ull) /*ABE*/ |
                              ((rnd() % 2) ? (1ull << 8) : 0ull) /*FST*/;
        gs.writeRegister(GS_REG_PRIM, prim);
        const int verts = sprite ? 2 : 3;
        for (int v = 0; v < verts; ++v)
        {
            const uint64_t rgba = (uint64_t)rnd() & 0xFFFFFFFFull;
            gs.writeRegister(GS_REG_RGBAQ, rgba | (0x3F800000ull << 32));
            gs.writeRegister(GS_REG_ST, (uint64_t)(0x3E000000u + (rnd() & 0x00FFFFFF)) | ((uint64_t)(0x3E000000u + (rnd() & 0x00FFFFFF)) << 32));
            gs.writeRegister(GS_REG_UV, (uint64_t)(rnd() & 0x3FFF) | ((uint64_t)(rnd() & 0x3FFF) << 16));
            const uint64_t x = (uint64_t)(rnd() % 700) << 4, y = (uint64_t)(rnd() % 500) << 4;
            gs.writeRegister(GS_REG_XYZ2, x | (y << 16) | ((uint64_t)rnd() << 32));
        }
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : vram) h = (h ^ b) * 1099511628211ull;
    std::printf("hash=%016llx time=%.1f ms\n", (unsigned long long)h, ms);
    return 0;
}
