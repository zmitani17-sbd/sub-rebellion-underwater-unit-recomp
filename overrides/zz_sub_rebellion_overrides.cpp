// Sub Rebellion (USA, SLUS-20548): game-specific runtime overrides.
//
// Copied into PS2Recomp's ps2xRuntime/src/runner/ next to the generated code. The "zz_" prefix
// keeps it in the last unity-build batch, so adding or editing it rebuilds only that batch.
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

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

    // PS2X_CD_TRACE=1: log the game's file-loading calls (its read queue, the racCdvd request
    // library and sceCdRead) with their arguments and results. Repeated identical polls are
    // logged once.
    std::string guestString(const uint8_t *rdram, uint32_t address)
    {
        std::string text;
        const uint32_t offset = address & PS2_RAM_MASK;
        for (uint32_t i = 0; i < 48u && offset + i < PS2_RAM_SIZE; ++i)
        {
            const char ch = static_cast<char>(rdram[offset + i]);
            if (ch == '\0')
                break;
            if (ch < 0x20 || ch > 0x7e)
                return {};
            text.push_back(ch);
        }
        return text.size() >= 3u ? text : std::string{};
    }

    uint32_t guestWord(const uint8_t *rdram, uint32_t address)
    {
        uint32_t value = 0u;
        std::memcpy(&value, rdram + (address & PS2_RAM_MASK & ~3u), sizeof(value));
        return value;
    }

    uint16_t guestHalf(const uint8_t *rdram, uint32_t address)
    {
        uint16_t value = 0u;
        std::memcpy(&value, rdram + (address & PS2_RAM_MASK & ~1u), sizeof(value));
        return value;
    }

    // Where the game's read module (readOpen/readPolling) holds file `index`: the loaded list at
    // gp-0x6A80, the unreferenced list at gp-0x6A7C or the request queue at gp-0x6A78.
    std::string describeReadEntry(const uint8_t *rdram, uint32_t gp, uint32_t index)
    {
        char text[160];
        const uint32_t lists[2] = {gp - 0x6A80u, gp - 0x6A7Cu};
        const char *names[2] = {"loaded", "unreferenced"};
        for (uint32_t l = 0; l < 2u; ++l)
        {
            uint32_t node = guestWord(rdram, lists[l]);
            for (uint32_t guard = 0; node != 0u && guard < 4096u; ++guard, node = guestWord(rdram, node))
            {
                if (guestHalf(rdram, node + 0xEu) == index)
                {
                    std::snprintf(text, sizeof(text), " [%s node=0x%x buf=0x%x pending=%u refs=%u req-state=%u]", names[l], node,
                                  guestWord(rdram, node + 8u), guestHalf(rdram, node + 0xCu), guestWord(rdram, node + 0x10u),
                                  guestWord(rdram, node + 0x14u));
                    return text;
                }
            }
        }
        uint32_t node = guestWord(rdram, gp - 0x6A78u);
        for (uint32_t guard = 0; node != 0u && guard < 4096u; ++guard, node = guestWord(rdram, node + 4u))
        {
            if (guestHalf(rdram, node + 8u) == index)
            {
                std::snprintf(text, sizeof(text), " [queued node=0x%x refs=%u issued=%u state=%u]", node, guestWord(rdram, node + 0xCu),
                              guestHalf(rdram, node + 0x14u), guestHalf(rdram, node + 0x16u));
                return text;
            }
        }
        return " [absent]";
    }

    // Mission start without the game's data. NEW GAME sets MissionReturnFlag (title_main ->
    // Set_MissionReturnFlag(1)), and with it set, mission_wait (0x126A80) builds the stage
    // (CSTG_Create -> CFPO_Create/CFDO_Create) as soon as mission_title_main sets flag 0x20000,
    // without the readDoneCheckAll()/CTM_IsAllTextureReady() wait it does otherwise. The title card
    // sets that flag about 60 frames in, while the read queue (one file per CdvdProc tick) still
    // holds most of the ~250 files that MOD_Load and CFM_Load just queued. readGetAdr() then
    // returns 0 for the stage's models and CCDO_GetPositionInfoFromFile parses "text" from address
    // 0, writing NULs over the bottom 8 MB of RAM (the hang at "MISSION 01 / NOW LOADING").
    // Keep mission_wait waiting until the read queue is drained and the textures are in, as its
    // other path does. Both checks below are the game's own, without their side effects.
    constexpr uint32_t kMissionWait = 0x126a80u;
    PS2Runtime::RecompiledFunction g_missionWaitOriginal = nullptr;

    bool allReadsDone(const uint8_t *rdram, uint32_t gp)
    {
        // readDoneCheckAll (0x10B2A8): nothing still queued at gp-0x6A78, and every loaded entry
        // (list at gp-0x6A80) either not pending or with its racCdvd request (+0x14) in state 3.
        if (guestWord(rdram, gp - 0x6A78u) != 0u)
            return false;
        uint32_t node = guestWord(rdram, gp - 0x6A80u);
        for (uint32_t guard = 0; node != 0u && guard < 65536u; ++guard, node = guestWord(rdram, node))
        {
            if (guestHalf(rdram, node + 0xCu) != 0u && guestHalf(rdram, node + 0x14u) != 3u)
                return false;
        }
        return true;
    }

    bool allTexturesReady(const uint8_t *rdram, uint32_t gp)
    {
        // CTM_IsAllTextureReady (0x10A938): flag 2 on every entry of the list at gp-0x6A88, and
        // flag 4 on every entry of the six lists at 0x26A650.
        uint32_t node = guestWord(rdram, gp - 0x6A88u);
        for (uint32_t guard = 0; node != 0u && guard < 65536u; ++guard, node = guestWord(rdram, node + 4u))
        {
            if ((guestHalf(rdram, node + 0x10u) & 2u) == 0u)
                return false;
        }
        for (uint32_t list = 0; list < 6u; ++list)
        {
            node = guestWord(rdram, 0x26a650u + 4u * list);
            for (uint32_t guard = 0; node != 0u && guard < 65536u; ++guard, node = guestWord(rdram, node + 4u))
            {
                if ((guestHalf(rdram, node + 0x14u) & 4u) == 0u)
                    return false;
            }
        }
        return true;
    }

    void missionWaitForData(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t gp = GPR_U32(ctx, 28);
        const bool returnFlag = rdram[(gp - 0x56C4u) & PS2_RAM_MASK] != 0u; // MissionReturnFlag
        if (returnFlag && !(allReadsDone(rdram, gp) && allTexturesReady(rdram, gp)))
        {
            static bool s_logged = false;
            if (!s_logged)
            {
                s_logged = true;
                std::cerr << "[sub-rebellion] mission start: waiting for the stage data to load" << std::endl;
            }
            ctx->pc = GPR_U32(ctx, 31); // what mission_wait does while it waits: return
            return;
        }
        g_missionWaitOriginal(rdram, ctx, runtime);
    }

    template <uint32_t Address>
    struct CdTrace
    {
        static inline const char *name = "";
        static inline PS2Runtime::RecompiledFunction original = nullptr;
        static inline uint32_t dedupe = 0u; // 0: every call; 1: when the result changes; 2: per (a0, a1) and result
        static inline bool describeFile = false;
        static inline bool showMissionFlags = false;

        static void call(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t a[4] = {GPR_U32(ctx, 4), GPR_U32(ctx, 5), GPR_U32(ctx, 6), GPR_U32(ctx, 7)};
            const uint32_t ra = GPR_U32(ctx, 31);
            const uint32_t gp = GPR_U32(ctx, 28);
            std::string strings;
            for (uint32_t i = 0; i < 4u; ++i)
            {
                const std::string text = (a[i] >= 0x100000u && a[i] < PS2_RAM_SIZE) ? guestString(rdram, a[i]) : std::string{};
                if (!text.empty())
                    strings += " a" + std::to_string(i) + "=\"" + text + "\"";
            }
            if (describeFile)
                strings += " before:" + describeReadEntry(rdram, gp, a[0] >> 16);
            if (showMissionFlags)
            {
                char flags[96];
                std::snprintf(flags, sizeof(flags), " returnFlag=%u flags268cd0=0x%x queueHead=0x%x", rdram[(gp - 0x56C4u) & PS2_RAM_MASK],
                              guestWord(rdram, 0x268cd0u), guestWord(rdram, gp - 0x6A78u));
                strings += flags;
            }
            original(rdram, ctx, runtime);
            const uint32_t v0 = GPR_U32(ctx, 2);
            if (describeFile)
                strings += " after:" + describeReadEntry(rdram, gp, a[0] >> 16);
            if (dedupe)
            {
                static std::map<uint64_t, uint32_t> s_last;
                const uint64_t key = dedupe >= 2u ? ((static_cast<uint64_t>(a[0]) << 32) | a[1]) : 0u;
                const auto found = s_last.find(key);
                if (found != s_last.end() && found->second == v0)
                    return;
                s_last[key] = v0;
            }
            static uint32_t s_count = 0u;
            if (s_count++ >= 20000u)
                return;
            std::fprintf(stderr, "[cd-trace] %s(0x%x, 0x%x, 0x%x, 0x%x) ra=0x%x -> 0x%x%s\n", name, a[0], a[1], a[2], a[3], ra,
                         v0, strings.c_str());
        }
    };

    template <uint32_t Address>
    void traceCd(PS2Runtime &runtime, const char *name, uint32_t dedupe = 0u, bool describeFile = false, bool showMissionFlags = false)
    {
        CdTrace<Address>::showMissionFlags = showMissionFlags;
        CdTrace<Address>::name = name;
        CdTrace<Address>::dedupe = dedupe;
        CdTrace<Address>::describeFile = describeFile;
        CdTrace<Address>::original = runtime.lookupFunction(Address);
        if (!CdTrace<Address>::original || !runtime.replaceFunction(Address, &CdTrace<Address>::call))
            std::fprintf(stderr, "[cd-trace] could not wrap %s\n", name);
    }

    void installCdTrace(PS2Runtime &runtime)
    {
        traceCd<0x10afd0u>(runtime, "readOpen", 0u, true);
        traceCd<0x10b030u>(runtime, "readClose", 0u, true);
        traceCd<0x10b120u>(runtime, "readDoneCheck", 2u);
        traceCd<0x10b2a8u>(runtime, "readDoneCheckAll", 1u);
        traceCd<0x10a938u>(runtime, "CTM_IsAllTextureReady", 1u);
        traceCd<0x1260e8u>(runtime, "MissionLoad", 0u, false, true);
        traceCd<0x126a80u>(runtime, "mission_wait", 1u, false, true);
        traceCd<0x1566b0u>(runtime, "CSTG_Create", 0u, false, true);
        traceCd<0x10b1a8u>(runtime, "readGetAdr", 2u, true);
        traceCd<0x10b318u>(runtime, "readGetSize", 2u);
        traceCd<0x1ed778u>(runtime, "racCdvdRequestSchFile");
        traceCd<0x1ed9b8u>(runtime, "racCdvdRequestName");
        traceCd<0x1eda58u>(runtime, "racCdvdRequestSec");
        traceCd<0x1edb18u>(runtime, "racCdvdRequestNameStm");
        traceCd<0x1edbd0u>(runtime, "racCdvdRequestSecStm");
        traceCd<0x1edcb0u>(runtime, "racCdvdRequestNameInl");
        traceCd<0x1edd70u>(runtime, "racCdvdRequestSecInl");
        traceCd<0x1ede70u>(runtime, "racCdvdSync", 2u);
        traceCd<0x1ed528u>(runtime, "racCdvdResultError", 2u);
        traceCd<0x1efce8u>(runtime, "racDbReadCdFile");
        traceCd<0x1fdcf8u>(runtime, "sceCdRead");
        std::fprintf(stderr, "[cd-trace] installed\n");
    }

    void applySubRebellion(PS2Runtime &runtime)
    {
        g_missionWaitOriginal = runtime.lookupFunction(kMissionWait);
        if (!g_missionWaitOriginal || !runtime.replaceFunction(kMissionWait, &missionWaitForData))
        {
            std::cerr << "[sub-rebellion] could not replace mission_wait" << std::endl;
        }

        if (std::getenv("PS2X_CD_TRACE"))
        {
            installCdTrace(runtime);
        }

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
        // The 512x448 frame fills a 4:3 TV; show it at 4:3 rather than with square pixels.
        if (!std::getenv("PS2X_ASPECT"))
        {
#ifdef _WIN32
            _putenv("PS2X_ASPECT=4:3");
#else
            setenv("PS2X_ASPECT", "4:3", 0);
#endif
        }

        if (!std::getenv("PS2X_PLAY_MOVIES") && !runtime.replaceFunction(kRacMoviePlay, &racMoviePlaySkip))
        {
            std::cerr << "[sub-rebellion] could not replace racMoviePlay" << std::endl;
        }
    }
}

PS2_REGISTER_GAME_OVERRIDE("Sub Rebellion: skip movies (no IPU), wait for mission data", "SLUS_205.48", 0u, 0u, &applySubRebellion);
