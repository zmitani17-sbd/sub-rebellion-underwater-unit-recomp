// Sub Rebellion (USA, SLUS-20548): game-specific runtime overrides.
//
// Copied into PS2Recomp's ps2xRuntime/src/runner/ next to the generated code. The "zz_" prefix
// keeps it in the last unity-build batch, so adding or editing it rebuilds only that batch.
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace
{
    constexpr uint32_t kRacMoviePlay = 0x114218u;

    // racMoviePlay(parent, movieId, bufferSize, flags) adds the movie object as a child of
    // `parent`. Its MovieProc thread feeds the .DSI stream to the IPU, which PS2Recomp doesn't
    // emulate, so the movie never ends and the screen stays black. Act as racMoviePlay does for an
    // out-of-range movie id: return 0 without adding the child. No caller uses the result.
    void racMoviePlaySkip(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        (void)rdram;
        (void)runtime;
        static uint32_t s_skipped = 0u;
        if (s_skipped++ < 16u)
        {
            std::cerr << "[sub-rebellion] skipping movie " << GPR_U32(ctx, 5) << std::endl;
        }
        SET_GPR_U32(ctx, 2, 0u);
        ctx->pc = GPR_U32(ctx, 31);
    }

    void applySubRebellion(PS2Runtime &runtime)
    {
        // Write log output straight through, so log.txt is current while the game runs (the play
        // script redirects stdout to it, which would otherwise hold output in a buffer).
        std::setvbuf(stdout, nullptr, _IONBF, 0);

        // The game outputs interlaced video but draws full 448-line frames, so show each frame
        // whole instead of alternate lines per field (which bounces by a line every refresh).
        if (!std::getenv("PS2X_DEINTERLACE"))
        {
#ifdef _WIN32
            _putenv("PS2X_DEINTERLACE=weave");
#else
            setenv("PS2X_DEINTERLACE", "weave", 0);
#endif
        }

        if (std::getenv("PS2X_PLAY_MOVIES"))
        {
            return;
        }
        if (!runtime.replaceFunction(kRacMoviePlay, &racMoviePlaySkip))
        {
            std::cerr << "[sub-rebellion] could not replace racMoviePlay" << std::endl;
        }
    }
}

PS2_REGISTER_GAME_OVERRIDE("Sub Rebellion: skip movies (no IPU)", "SLUS_205.48", 0u, 0u, &applySubRebellion);
