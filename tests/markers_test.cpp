// Exercise production capture and drawing with controlled game data and a CPU
// clear-attachment target. These checks do not replace multiplayer acceptance.
#include "../src/dllmain.cpp"
#include <iostream>
#include <limits>

namespace
{
    void Check(bool ok, const char* message)
    {
        if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
        std::cout << "PASS: " << message << '\n';
    }
    template<class T> void Put(void* target, std::size_t offset, T value)
    {
        std::memcpy(static_cast<unsigned char*>(target) + offset, &value, sizeof(value));
    }
    struct Transform { std::int64_t xyz[3]; float q[4]; } transforms[1024] = {};
    std::array<unsigned char, 1023 * 0x60> basicPlayers{};
    std::uint32_t localPlayer[2] = { 0, 9 };
    uintptr_t context[2] = { 0x1234, 27 };
    WaypointsUiIterationRecord waypointRecord{};
    bool badLookup = false;
    int lookups = 0;
    unsigned char waypointComponent[0x20] = {};
    bool waypointComponentPresent = true;

    void __fastcall Init(void* ctx, void* data, std::uint32_t size)
    {
        static_cast<uintptr_t*>(ctx)[1] = 500; // Must never affect the game's cursor.
        if (size == 0x98)
        {
            auto* record = static_cast<uintptr_t*>(data);
            record[1] = reinterpret_cast<uintptr_t>(basicPlayers.data());
            record[2] = reinterpret_cast<uintptr_t>(localPlayer);
            record[6] = 0x5678;
        }
        else if (size == sizeof(waypointRecord))
            std::memcpy(data, &waypointRecord, size);
        else Check(false, "unexpected iteration record size");
    }
    uintptr_t __fastcall World(uintptr_t descriptor)
    {
        Check(descriptor == context[0], "world lookup uses the ECS descriptor");
        return 0xABCD;
    }
    uintptr_t* __fastcall Lookup(uintptr_t* output, uintptr_t world, uintptr_t lookup, std::uint32_t id)
    {
        ++lookups;
        if (lookup == 0x9876)
        {
            Check(world == 0xABCD && id == localPlayer[1], "waypoint lookup selects the local player's component");
            output[0] = waypointComponentPresent ? reinterpret_cast<uintptr_t>(waypointComponent) : 0;
            output[1] = sizeof(waypointComponent);
            return output;
        }
        badLookup |= world != 0xABCD || lookup != 0x5678 || id == localPlayer[1];
        output[0] = transforms[id].xyz[0] ? reinterpret_cast<uintptr_t>(&transforms[id]) : 0;
        output[1] = sizeof(Transform); // The real API writes two words.
        return output;
    }
    void CapturePlayers()
    {
        g_lastRemoteCaptureTick = GetTickCount() - 100;
        CapturePlayerUiDataHook(context, nullptr, nullptr, nullptr);
    }
    struct Pixel { float r = 0.2f, g = 0.3f, b = 0.4f; } pixels[128][128];
    int clearCalls = 0;
    void Raster(void*, std::uint32_t, const VkClearAttachment* a, std::uint32_t count, const VkClearRect* rects)
    {
        ++clearCalls;
        for (std::uint32_t i = 0; i < count; ++i)
            for (std::uint32_t dy = 0; dy < rects[i].rect.extent.height; ++dy)
                for (std::uint32_t dx = 0; dx < rects[i].rect.extent.width; ++dx)
                {
                    const int x = rects[i].rect.offset.x + dx, y = rects[i].rect.offset.y + dy;
                    if (x < 0 || x >= 128 || y < 0 || y >= 128)
                        Check(false, "clear rectangle stays on target");
                    pixels[y][x] = { a->clearValue.color.float32[0], a->clearValue.color.float32[1], a->clearValue.color.float32[2] };
                }
    }
    void WritePreview(const char* path)
    {
        BITMAPFILEHEADER header{};
        BITMAPINFOHEADER info{};
        header.bfType = 0x4D42;
        header.bfOffBits = sizeof(header) + sizeof(info);
        header.bfSize = header.bfOffBits + 128 * 128 * 4;
        info.biSize = sizeof(info); info.biWidth = 128; info.biHeight = -128;
        info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(&info), sizeof(info));
        for (const auto& row : pixels)
            for (const auto& pixel : row)
            {
                const unsigned char bgra[] = { static_cast<unsigned char>(pixel.b * 255),
                    static_cast<unsigned char>(pixel.g * 255), static_cast<unsigned char>(pixel.r * 255), 255 };
                file.write(reinterpret_cast<const char*>(bgra), sizeof(bgra));
            }
        Check(file.good(), "write optional marker preview");
    }
}

int main(int argc, char** argv)
{
    g_liveMarkerLayoutSupported = true;
    g_directCameraTrackingSupported = true;
    g_iterInit = Init;
    g_getEcsWorld = World;
    g_lookupComponent = Lookup;
    Put(basicPlayers.data(), 2 * 0x60, std::uint32_t(3));
    Put(basicPlayers.data(), 6 * 0x60, std::uint32_t(7));
    Put(basicPlayers.data(), 8 * 0x60, std::uint32_t(9));
    Put(basicPlayers.data(), 10 * 0x60, std::uint32_t(999)); // Reject mismatched slot.
    transforms[3] = { { WorldToFixed(3810), WorldToFixed(825), WorldToFixed(1898) }, { 0, 0, 0, 1 } };
    transforms[7] = transforms[3];
    CapturePlayers();
    Check(context[1] == 27 && !badLookup && lookups == 2, "capture leaves cursor intact and excludes local/invalid IDs");
    Check(g_remotePlayers.size() == 2 && g_remotePlayers[0].id == 3 && g_remotePlayers[1].id == 7,
        "co-located players remain separate and pings are not players");
    transforms[3].xyz[0] = WorldToFixed(3900);
    transforms[3].q[1] = transforms[3].q[3] = 0.70710678f;
    transforms[7].xyz[0] = 0; // No longer present in the ECS world.
    CapturePlayers();
    Check(g_remotePlayers.size() == 1 && g_remotePlayers[0].x == WorldToFixed(3900) &&
        std::fabs(g_remotePlayers[0].heading - 1.5707963f) < 0.0001f, "movement and facing update while departed player disappears");
    transforms[3].q[1] = std::numeric_limits<float>::quiet_NaN();
    CapturePlayers();
    Check(g_remotePlayers.empty(), "reject invalid orientation without retaining old marker");
    transforms[3].q[1] = 0; transforms[3].q[3] = 1;
    transforms[3].xyz[0] = WorldToFixed(50000);
    CapturePlayers();
    Check(g_remotePlayers.empty(), "reject off-map remote transform");
    std::size_t count = 42;
    MinimapLive::Marker output[MinimapLive::MaxPlayers]{};
    Check(!ReadRemotePlayers(reinterpret_cast<void*>(1), output, count, 0) && count == 0,
        "unreadable game context fails without publishing partial data");
    g_liveMarkerLayoutSupported = false;
    Check(!ReadRemotePlayers(context, output, count, 0), "unsupported client skips foreign calls");
    g_liveMarkerLayoutSupported = true;

    unsigned char list[0x30] = {}, events[0x50] = {};
    Put(list, 0x18, reinterpret_cast<uintptr_t>(events)); Put(list, 0x28, std::uint32_t(2));
    Put(events, 8, std::uint32_t(3)); Put(events, 0x30, std::uint32_t(9));
    std::int64_t xyz[3] = { WorldToFixed(3810), WorldToFixed(825), WorldToFixed(1898) };
    std::memcpy(events + 0x10, xyz, sizeof(xyz)); std::memcpy(events + 0x38, xyz, sizeof(xyz));
    CapturePingEvents(list, 9, 100);
    Check(g_pings.size() == 1 && g_pings[0].id == 3, "remote ping feed excludes the local echo");
    Put(events, 0x10, WorldToFixed(3900));
    CapturePingEvents(list, 0, 200);
    Check(g_pings.size() == 2 && g_pings[0].x == WorldToFixed(3900), "local input supported and repeated sender replaces old ping");
    Put(list, 0x28, std::uint32_t(1025)); CapturePingEvents(list, 0, 300);
    Check(g_pings.size() == 2, "reject unbounded ping list");
    std::vector<MinimapLive::Marker> players, pings;
    g_remotePlayers = { { 7, xyz[0], xyz[1], xyz[2], 200 } };
    CopyLiveMarkers(players, pings, 2200);
    Check(players.empty() && pings.size() == 2, "players expire independently of pings");
    CopyLiveMarkers(players, pings, 8200);
    Check(pings.empty(), "pings expire after eight seconds");
    MinimapLive::Marker wrapped{}; wrapped.updated = 0xFFFFFFF0u;
    Check(MinimapLive::Fresh(wrapped, 20, 100) && !MinimapLive::Fresh(wrapped, 100, 100), "expiry handles timer wrap");
    for (std::uint32_t i = 1; i <= 100; ++i) { wrapped.id = i; MinimapLive::UpsertPing(g_pings, wrapped); }
    Check(g_pings.size() == MinimapLive::MaxPings, "ping history remains bounded");
    ClearLiveMarkers();

    std::vector<unsigned char> state(0x305560);
    unsigned char waypoint[0xF0] = {};
    Put(state.data(), 0x305550, reinterpret_cast<uintptr_t>(waypoint));
    Put(state.data(), 0x305558, std::uint64_t(1));
    Put(waypoint, 0, std::uint32_t(9));
    std::memcpy(waypoint + 0x30, xyz, sizeof(xyz));
    waypointRecord.state = state.data(); waypointRecord.unknown8 = localPlayer;
    waypointRecord.lookupContext = reinterpret_cast<void*>(0x9876);
    std::memcpy(waypointComponent + 8, xyz, sizeof(xyz));
    Check(TryCaptureWaypointsFromPlayerWaypointsUi(context) && CopyWaypoints().empty(), "nonzero player ID is not an active waypoint");
    waypointComponent[0] = 1;
    TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(CopyWaypoints().size() == 1 && context[1] == 27 && waypoint[0x48] == 0,
        "active waypoint captured while UI cache is still cleared at system entry");
    waypointRecord.state = nullptr;
    TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(CopyWaypoints().size() == 1, "local waypoint does not require the UI player list");
    waypointRecord.state = state.data();
    Put(waypointComponent, 8, WorldToFixed(4000)); TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(CopyWaypoints().size() == 1 && CopyWaypoints()[0].x == WorldToFixed(4000), "moving waypoint replaces old position");
    waypoint[0x48] = 1; // Stale UI data must not resurrect a cleared destination.
    waypointComponent[0] = 0; TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(CopyWaypoints().empty(), "clearing waypoint removes marker despite retained coordinates");
    waypointComponent[0] = 1; TryCaptureWaypointsFromPlayerWaypointsUi(context);
    g_lastWaypointTick = GetTickCount() - MinimapLive::PlayerStaleMs;
    Check(CopyWaypoints().empty(), "interrupted waypoint feed expires");
    waypointComponentPresent = false;
    TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(g_waypoints.empty(), "missing local waypoint component clears snapshot");
    waypointComponentPresent = true;
    Put(waypointComponent, 8, WorldToFixed(50000));
    TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(g_waypoints.empty(), "invalid local waypoint coordinates clear snapshot");
    g_liveMarkerLayoutSupported = false;
    Put(state.data(), 0x305558, std::uint64_t(WAYPOINT_MAX_ENTRIES + 1));
    TryCaptureWaypointsFromPlayerWaypointsUi(context);
    Check(g_waypoints.empty(), "legacy invalid waypoint count clears snapshot");
    g_liveMarkerLayoutSupported = true;

    ModContext config{};
    config.config.GetString = [](const char*, const char* key, std::string fallback) {
        return std::string(key).find("show_") == 0 ? std::string("false") : fallback;
    };
    RefreshMinimapConfig(&config);
    Check(!g_showOtherPlayers && !g_showPings && !g_showWaypoints, "independent visibility options read from config");
    g_remotePlayers = { { 3, xyz[0], xyz[1], xyz[2], 100 } }; g_pings = g_remotePlayers;
    CopyLiveMarkers(players, pings, 100);
    Check(players.empty() && pings.empty(), "hidden live markers never reach drawing snapshot");
    g_showPings = true;
    CopyLiveMarkers(players, pings, 100);
    Check(players.empty() && pings.size() == 1, "pings remain visible with players disabled");
    g_showOtherPlayers = true; g_showPings = false;
    CopyLiveMarkers(players, pings, 100);
    Check(players.size() == 1 && pings.empty(), "players remain visible with pings disabled");
    g_showOtherPlayers = true; g_showPings = true; g_showWaypoints = true;
    ResetWorldSessionFromLog();
    Check(g_remotePlayers.empty() && g_pings.empty() && g_waypoints.empty(), "world exit clears every live marker feed");

    int px = 0, py = 0; bool clipped = false;
    ProjectWorldToMinimap(1020, 1000, 1000, 1000, 2, 0, 64, 64, 60, px, py, clipped);
    Check(px == 74 && py == 64 && !clipped, "world position respects minimap zoom");
    ProjectWorldToMinimap(1020, 1000, 1000, 1000, 2, 1.5707963f, 64, 64, 60, px, py, clipped);
    Check(std::abs(px - 64) <= 1 && std::abs(py - 54) <= 1, "world position rotates with camera");
    ProjectWorldToMinimap(5000, 1000, 1000, 1000, 2, 0, 64, 64, 60, px, py, clipped);
    Check(clipped && px == 106, "distant marker clamps to minimap rim");
    VulkanMinimapRenderer renderer{};
    renderer.width = renderer.height = 128; renderer.fns.cmdClearAttachments = Raster;
    DrawWaypointDiamond(renderer, nullptr, 64, 64);
    Check(pixels[64][64].r == 0.2f && pixels[64][70].b == 0.4f && pixels[64][79].r == 1.0f,
        "yellow waypoint outline preserves centre and underlying icon pixels");
    DrawPingDiamond(renderer, nullptr, 32, 32);
    Check(pixels[32][38].g > pixels[32][38].r && pixels[32][32].g < 0.2f,
        "green ping diamond has a dark arrow centre");
    g_playerArrow.attempted = true; // Exercise procedural fallback when the asset is absent.
    clearCalls = 0;
    DrawRemotePlayerArrow(renderer, nullptr, 96, 32, 1.5707963f);
    Check(clearCalls <= 8 && pixels[32][105].g > pixels[32][105].r && pixels[21][96].r == 0.2f,
        "green player arrow rotates and batches draw commands");

    Check(TryLoadPlayerArrowFromPath("assets/embervale_player_arrow.rgba", g_playerArrow), "load the actual local-player arrow asset");
    for (auto& row : pixels) for (auto& pixel : row) pixel = {};
    clearCalls = 0;
    DrawRemotePlayerArrow(renderer, nullptr, 24, 64, 0);
    Check(clearCalls > 0 && clearCalls <= 8, "actual player artwork uses bounded green palette batches");
    bool greenPixel = false;
    for (const auto& row : pixels)
        for (const auto& pixel : row)
            if (pixel.g > 0.5f && pixel.g > pixel.r && pixel.g > pixel.b) greenPixel = true;
    Check(greenPixel, "actual player artwork is tinted green");
    DrawPingDiamond(renderer, nullptr, 64, 64);
    // Simulate a map icon underneath the waypoint. The outline must preserve it.
    CmdClearRect(renderer, nullptr, 0.4f, 0.7f, 0.9f, 1.0f, 101, 61, 7, 7);
    DrawWaypointDiamond(renderer, nullptr, 104, 64);
    Check(pixels[64][104].b == 0.9f, "waypoint preserves a previously drawn POI icon");
    if (argc > 1) WritePreview(argv[1]);
    return 0;
}
