// Production readers and drawing commands, without hooking or modifying a game.
#include "../src/dllmain.cpp"
#include <iostream>

namespace
{
    void Check(bool ok, const char* message)
    {
        if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
        std::cout << "PASS: " << message << '\n';
    }
    template<class T> void Put(void* data, std::size_t offset, T value)
    { std::memcpy(static_cast<std::uint8_t*>(data) + offset, &value, sizeof(value)); }
    std::int64_t Fixed(int value) { return static_cast<std::int64_t>(value) * 4294967296LL; }
    std::vector<std::uint8_t> state(DEATH_MAP_ARRAY_OFFSET + 32);
    std::array<std::uint8_t, WORLD_MAP_MARKER_STRIDE * 5> world{};
    std::array<std::uint8_t, WORLD_MAP_MARKER_STRIDE> custom{};
    void Entry(void* data, std::size_t index, int x, int z, std::uint32_t key)
    {
        const auto offset = index * WORLD_MAP_MARKER_STRIDE;
        Put(data, offset + 0x10, Fixed(x)); Put(data, offset + 0x18, Fixed(200));
        Put(data, offset + 0x20, Fixed(z)); Put(data, offset + 0x30, key);
    }
    void __fastcall InitMap(void* ctx, void* record, std::uint32_t size)
    {
        if (size != 16) std::exit(2);
        static_cast<uintptr_t*>(ctx)[1] = 999;
        static_cast<uintptr_t*>(record)[1] = reinterpret_cast<uintptr_t>(state.data());
    }
    struct Pixel { float r = 0, g = 0, b = 0; } pixels[320][512];
    int clears = 0;
    void Raster(void*, std::uint32_t, const VkClearAttachment* a, std::uint32_t count, const VkClearRect* rects)
    {
        ++clears;
        for (std::uint32_t i = 0; i < count; ++i)
            for (std::uint32_t dy = 0; dy < rects[i].rect.extent.height; ++dy)
                for (std::uint32_t dx = 0; dx < rects[i].rect.extent.width; ++dx)
                {
                    int x = rects[i].rect.offset.x + dx, y = rects[i].rect.offset.y + dy;
                    if (x < 0 || x >= 512 || y < 0 || y >= 320) std::exit(3);
                    pixels[y][x] = { a->clearValue.color.float32[0], a->clearValue.color.float32[1], a->clearValue.color.float32[2] };
                }
    }
    void WritePreview(const char* path)
    {
        BITMAPFILEHEADER h{}; BITMAPINFOHEADER info{};
        h.bfType = 0x4D42; h.bfOffBits = sizeof(h) + sizeof(info); h.bfSize = h.bfOffBits + sizeof(pixels) / sizeof(Pixel) * 4;
        info.biSize = sizeof(info); info.biWidth = 512; info.biHeight = -320;
        info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(&h), sizeof(h)); out.write(reinterpret_cast<const char*>(&info), sizeof(info));
        for (const auto& row : pixels) for (const auto& p : row)
        {
            const std::uint8_t color[] = { static_cast<std::uint8_t>(p.b * 255), static_cast<std::uint8_t>(p.g * 255), static_cast<std::uint8_t>(p.r * 255), 255 };
            out.write(reinterpret_cast<const char*>(color), 4);
        }
        Check(out.good(), "CPU preview written");
    }
    void CheckDeaths(VulkanMinimapRenderer& renderer)
    {
        std::array<std::uint8_t, WORLD_MAP_MARKER_STRIDE * 3> deaths{};
        const uintptr_t ui = reinterpret_cast<uintptr_t>(state.data());
        const DWORD now = GetTickCount();
        Entry(deaths.data(), 0, 1020, 1000, 0x1AD8F96E);
        Entry(deaths.data(), 1, 1000, 1040, 0x1AD8F96E);
        Entry(deaths.data(), 2, -10000, 1000, 0x1AD8F96E);
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET, reinterpret_cast<uintptr_t>(deaths.data()));
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{3});
        g_deathMarkerLayoutSupported = g_worldMapLayoutSupported = true;
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now).size() == 2, "multiple tombstones are captured; invalid coordinates are omitted");
        Check(CopyWorldMapMarkers(now).size() == 1, "death capture preserves existing POIs");
        Check(CopyDeathMarkers(now + 2001).empty(), "interrupted tombstone feed expires");
        CaptureWorldMapMarkers(ui, 0xFFFFFFF0u);
        Check(CopyDeathMarkers(0x10).size() == 2, "tombstone freshness survives tick wrap");
        Entry(deaths.data(), 0, 1050, 1000, 0x1AD8F96E);
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now)[0].x == Fixed(1050), "tombstone positions update from each completed snapshot");
        g_showDeathMarkers = false;
        Check(CopyDeathMarkers(now).empty(), "death visibility toggle hides existing tombstones");
        g_showDeathMarkers = true;
        Check(CopyDeathMarkers(now).size() == 2, "death visibility toggle restores the current snapshot");

        // Exercise the real draw path: ordinary POI limits and visibility cannot
        // suppress deaths; even coincident tombstones remain separate records.
        Entry(deaths.data(), 1, 1050, 1000, 0x1AD8F96E);
        CaptureWorldMapMarkers(ui, GetTickCount());
        g_showWorldMarkers = false; g_minimapMaxDrawnPoints = 8;
        std::memset(pixels, 0, sizeof(pixels)); clears = 0;
        DrawLiveMarkers(renderer, nullptr, {}, 1000, 1000, 1, 0, 160, 160, 130);
        Check(clears == 4 && pixels[155][210].r > 0.9f, "death layer renders skulls independently of POI visibility, limits and deduplication");
        Check(pixels[159][207].r < 0.1f, "skull fallback keeps dark eye sockets");
        g_showDeathMarkers = false; clears = 0;
        DrawLiveMarkers(renderer, nullptr, {}, 1000, 1000, 1, 0, 160, 160, 130);
        Check(clears == 0, "disabled death layer issues no draw commands");
        g_showDeathMarkers = true; g_showWorldMarkers = true;
        g_minimapMaxDrawnPoints = MINIMAP_DEFAULT_MAX_DRAWN_POINTS;
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{1});
        Entry(deaths.data(), 0, 9000, 1000, 0x1AD8F96E);
        CaptureWorldMapMarkers(ui, GetTickCount());
        for (float heading : {0.0f, 1.5707963f, 3.1415926f, 4.7123889f})
        {
            for (float zoom : {0.5f, 12.0f})
            {
                std::memset(pixels, 0, sizeof(pixels)); clears = 0;
                DrawLiveMarkers(renderer, nullptr, {}, 1000, 1000, zoom, heading, 160, 160, 130);
                bool outside = false;
                for (int y=0; y<320; ++y) for (int x=0; x<512; ++x)
                    if ((x-160)*(x-160)+(y-160)*(y-160)>130*130 && pixels[y][x].r != 0) outside=true;
                Check(clears == 2 && !outside, "distant skull remains inside the rim through rotation and zoom");
            }
        }
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{0});
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now).empty(), "recovering the final tombstone removes its marker");
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{1});
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now).size() == 1, "a later death creates a fresh marker");
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{DEATH_MAP_MAX_ENTRIES + 1});
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now).empty() && CopyWorldMapMarkers(now).size() == 1,
            "oversized death array clears only deaths and preserves POIs");
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{1});
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET, uintptr_t{1});
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now).empty() && CopyWorldMapMarkers(now).size() == 1,
            "unreadable death array clears only deaths and preserves POIs");
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET, reinterpret_cast<uintptr_t>(deaths.data()));
        CaptureWorldMapMarkers(ui, now);
        ClearLiveMarkers();
        Check(CopyDeathMarkers(now).empty(), "world exit clears tombstones");
        g_deathMarkerLayoutSupported = false;
        CaptureWorldMapMarkers(ui, now);
        Check(CopyDeathMarkers(now).empty() && CopyWorldMapMarkers(now).size() == 1,
            "unsupported death layout leaves existing POI capture working");
        Put(state.data(), DEATH_MAP_ARRAY_OFFSET + 8, std::uint64_t{0});
        ClearLiveMarkers();
        g_deathMarkerLayoutSupported = true;
    }

    void CheckClientLayout(const char* path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        Check(input.good(), "open optional client executable read-only");
        std::vector<char> file(static_cast<std::size_t>(input.tellg())); input.seekg(0);
        Check(input.read(file.data(), file.size()).good() && file.size() > sizeof(IMAGE_DOS_HEADER), "read client executable");
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
        Check(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 &&
            static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) < file.size(), "client PE header bounds");
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
        Check(nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
            nt->OptionalHeader.SizeOfImage < 512 * 1024 * 1024, "client image is bounded x64 PE");
        std::vector<std::uint8_t> image(nt->OptionalHeader.SizeOfImage);
        const auto* sections = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            const auto& s = sections[i];
            Check(static_cast<std::uint64_t>(s.PointerToRawData) + s.SizeOfRawData <= file.size() &&
                static_cast<std::uint64_t>(s.VirtualAddress) + s.SizeOfRawData <= image.size(), "client section bounds");
            std::memcpy(image.data() + s.VirtualAddress, file.data() + s.PointerToRawData, s.SizeOfRawData);
        }
        const auto savedBase = g_exeBase; const auto savedSize = g_exeImageSize; const auto savedInit = g_iterInit;
        g_exeBase = reinterpret_cast<uintptr_t>(image.data()); g_exeImageSize = image.size();
        for (const auto rva : { 0x1D45390, 0x1D47820, 0x1D4C100 })
            for (int word : { 0, 2 })
            {
                uintptr_t value = 0; std::memcpy(&value, image.data() + rva + word * 8, 8);
                value = value - nt->OptionalHeader.ImageBase + g_exeBase;
                Put(image.data(), rva + word * 8, value);
            }
        g_iterInit = reinterpret_cast<IterInitFn>(g_exeBase + 0x8DA7C0);
        Check(HasVerifiedWorldMapLayout(), "production layout gate matches installed Steam executable");
        Check(HasVerifiedDeathMarkerLayout(), "death layout gate matches the installed Steam executable");
        for (auto rva : {0x29DE49, 0x2A9D8F, 0x2A9DB0, 0x29C3F3})
        {
            image[rva] ^= 1;
            Check(!HasVerifiedDeathMarkerLayout() && HasVerifiedWorldMapLayout(),
                "changed tombstone layout disables deaths without disabling POIs");
            image[rva] ^= 1;
        }
        image[0x2A0D34] ^= 1;
        Check(!HasVerifiedWorldMapLayout(), "changed marker stride disables the reader");
        image[0x2A0D34] ^= 1; image[0x29DE3B] ^= 1;
        Check(!HasVerifiedWorldMapLayout(), "changed UI array layout disables the reader");
        g_exeBase = savedBase; g_exeImageSize = savedSize; g_iterInit = savedInit;
    }
}

int main(int argc, char** argv)
{
    Check(TryLoadMinimapIconsFromPath("assets/embervale_minimap_icons.bin", g_minimapIcons), "load actual icon atlas");
    g_minimapIcons.attempted = true;
    const auto nativeIcon = std::find_if(g_minimapIcons.icons.begin(), g_minimapIcons.icons.end(), [](const auto& icon) { return icon.key > 0xFFFFu; });
    Check(nativeIcon != g_minimapIcons.icons.end(), "atlas contains native world-map keys");
    const auto iconKey = nativeIcon->key;
    Check(std::count(nativeIcon->glyph.begin(), nativeIcon->glyph.end(), 1) > 0 &&
        std::count(nativeIcon->glyph.begin(), nativeIcon->glyph.end(), 1) < 200, "native glyph excludes its dark tile background");
    Entry(world.data(), 0, 1000, 1000, WORLD_MAP_FLAME_ALTAR);
    Entry(world.data(), 1, 1100, 1000, 0);
    Entry(world.data(), 2, 1200, 1000, iconKey);
    Entry(world.data(), 3, 1300, 1000, WORLD_MAP_PING);
    Entry(world.data(), 4, 1400, 1000, 0xDEADBEEF);
    Entry(custom.data(), 0, 1500, 1000, iconKey);
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET, reinterpret_cast<uintptr_t>(world.data()));
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET + 8, std::uint64_t{5});
    Put(state.data(), CUSTOM_MAP_ARRAY_OFFSET, reinterpret_cast<uintptr_t>(custom.data()));
    Put(state.data(), CUSTOM_MAP_ARRAY_OFFSET + 8, std::uint64_t{1});
    const DWORD now = GetTickCount();
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now);
    Check(CopyWorldMapMarkers(now).size() == 6, "both completed map lists are captured");
    g_worldMapLayoutSupported = g_liveMarkerLayoutSupported = true;
    std::vector<MinimapWorldPoint> points;
    BuildWorldPoints(points, {}, {}, {});
    Check(points.size() == 5, "world-map mirror excludes the dedicated live ping duplicate");
    const auto countKind = [&](std::uint32_t kind) { return std::count_if(points.begin(), points.end(), [kind](const auto& p) { return p.kind == kind; }); };
    Check(countKind(11) == 1 && countKind(12) == 1 && countKind(31) == 1 && countKind(iconKey) == 1 && countKind(55) == 1,
        "custom pin, NPC fallback, altar, native icon and unknown fallback remain distinct");
    Entry(custom.data(), 0, 1200, 1000, iconKey);
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now);
    points.clear(); BuildWorldPoints(points, {}, {}, {});
    Check(points.size() == 4 && countKind(11) == 1 && countKind(iconKey) == 0, "custom pin takes priority over coincident POI");
    Check(CopyWorldMapMarkers(now + 2001).empty(), "interrupted world-map feed expires");
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), 0xFFFFFFF0u);
    Check(CopyWorldMapMarkers(0x10).size() == 6, "world-map freshness survives tick wrap");
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET + 8, std::uint64_t{0});
    Put(state.data(), CUSTOM_MAP_ARRAY_OFFSET + 8, std::uint64_t{0});
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now);
    Check(g_worldMapValid && CopyWorldMapMarkers(now).empty(), "empty frame removes all earlier map markers");
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET + 8, std::uint64_t{WORLD_MAP_MAX_ENTRIES + 1});
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now);
    Check(!g_worldMapValid && CopyWorldMapMarkers(now).empty(), "oversized array is rejected before allocation or traversal");
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET + 8, std::uint64_t{1});
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET, uintptr_t{1});
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now);
    Check(!g_worldMapValid, "unreadable marker pointer clears the feed");
    Put(state.data(), WORLD_MAP_ARRAY_OFFSET, reinterpret_cast<uintptr_t>(world.data()));
    Entry(world.data(), 0, -10000, 1000, iconKey);
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now);
    Check(g_worldMapValid && CopyWorldMapMarkers(now).empty(), "out-of-world coordinates are omitted");
    Entry(world.data(), 0, 1000, 1000, iconKey);
    uintptr_t context[] = { 0x1234, 7 };
    g_iterInit = InitMap; g_lastWorldMapCaptureTick = GetTickCount() - 100;
    CaptureWorldMapHook(context, nullptr, nullptr, nullptr);
    Check(context[1] == 7 && CopyWorldMapMarkers(GetTickCount()).size() == 1, "named entry hook uses a private iterator context");
    g_worldMapLayoutSupported = false;
    ClearLiveMarkers(); CaptureWorldMapHook(context, nullptr, nullptr, nullptr);
    Check(CopyWorldMapMarkers(GetTickCount()).empty(), "unsupported layout does not invoke the reader");
    CaptureWorldMapMarkers(reinterpret_cast<uintptr_t>(state.data()), now); ClearLiveMarkers();
    Check(CopyWorldMapMarkers(now).empty(), "world exit clears world-map snapshots");

    ModContext config{};
    config.config.GetString = [](const char*, const char* key, std::string fallback) {
        if (std::strcmp(key, "map_light") == 0) return std::string("150");
        if (std::strcmp(key, "heading_smoothing_ms") == 0) return std::string("0");
        if (std::strcmp(key, "icon_style") == 0) return std::string("world-map");
        if (std::strcmp(key, "show_death_markers") == 0) return std::string("false");
        if (std::strcmp(key, "show_world_markers") == 0) return std::string("false");
        return fallback;
    };
    RefreshMinimapConfig(&config);
    Check(g_minimapMapLight == 100 && g_headingSmoothingMs == 0 && g_worldMapIconStyle && !g_showWorldMarkers,
        "runtime config enables gold icons and applies bounded display options");
    Check(!g_showDeathMarkers, "runtime config hides death markers");
    Check(g_showOtherPlayers && g_showPings && g_showWaypoints, "world-map option preserves independent live-marker settings");
    config.config.GetString = [](const char*, const char* key, std::string fallback) {
        return std::strcmp(key, "heading_smoothing_ms") == 0 ? std::string("-1") : fallback;
    };
    RefreshMinimapConfig(&config);
    Check(g_headingSmoothingMs == 55, "invalid negative smoothing restores its default");
    Check(ParseConfigInteger("1.5", 55, 0, 100) == 55 && ParseConfigInteger("150junk", 55, 0, 100) == 55,
        "numeric config rejects punctuation and trailing garbage");
    Check(ParseConfigInteger("999999999999999999999", 55, 0, 100) == 100 && ParseConfigInteger(" 0 ", 55, 0, 100) == 0,
        "numeric config handles large values and surrounding whitespace");
    RefreshMinimapConfig(nullptr);
    Check(g_minimapMapLight == 55 && g_headingSmoothingMs == 55 && !g_worldMapIconStyle && g_showWorldMarkers,
        "original icons and default display settings are restored without reinstalling");

    Check(g_showDeathMarkers, "death markers default to enabled without configuration");

    float r = 0.2f, g = 0.3f, b = 0.4f;
    ApplyMapLighting(r, g, b, 0.0f, 0);
    Check(std::fabs(r - 0.208f) < 0.0001f && std::fabs(b - 0.412f) < 0.0001f, "zero lighting retains original terrain treatment");
    float brightR = 0.2f, brightG = 0.3f, brightB = 0.4f;
    ApplyMapLighting(brightR, brightG, brightB, 0.0f, 55);
    Check(brightR > r && brightG > g && brightB > b && brightR <= 1, "default parchment lighting lifts terrain colors");
    r = g = b = 1.0f; ApplyMapLighting(r, g, b, 1.0f, 500);
    Check(r <= 1 && g <= 1 && b <= 1 && r > b, "lighting clamps input and warms the edge");
    VulkanMinimapRenderer renderer{};
    renderer.width = 512; renderer.height = 320; renderer.fns.cmdClearAttachments = Raster;
    CheckDeaths(renderer);
    Check(SmoothMapHeading(renderer, 3.1f, true, 100) == 3.1f, "first heading is immediate");
    Check(std::abs(SmoothMapHeading(renderer, -3.1f, true, 116)) > 3, "heading smoothing crosses the short arc at north");
    Check(SmoothMapHeading(renderer, 1, true, 2000) == 1, "stale heading resets after a pause");
    g_headingSmoothingMs = 0;
    Check(SmoothMapHeading(renderer, 2, true, 2001) == 2, "zero smoothing follows the camera immediately");
    g_headingSmoothingMs = 55; ClearLiveMarkers();
    Check(SmoothMapHeading(renderer, -1, true, 2002) == -1, "world re-entry resets smoothing");
    Check(SmoothMapHeading(renderer, 2, false, 2003) == 0, "missing heading returns to north");
    Check(MINIMAP_MAX_ZOOM == 7 && MINIMAP_MIN_ZOOM == -3, "zoom range includes four additional close-up steps");

    MinimapIcon glyph{}; glyph.width = glyph.height = 8; glyph.rgba.resize(8 * 8 * 4);
    for (int i : { 9, 10, 17, 18 }) for (int c = 0; c < 4; ++c) glyph.rgba[i * 4 + c] = 255;
    PrepareMapIconGlyph(glyph);
    Check(std::count(glyph.glyph.begin(), glyph.glyph.end(), 1) == 4, "transparent white glyph keeps its alpha shape");
    glyph.rgba.assign(8 * 8 * 4, 255); for (int c = 0; c < 3; ++c) glyph.rgba[9 * 4 + c] = 0;
    PrepareMapIconGlyph(glyph);
    Check(glyph.glyph[9] == 1 && glyph.glyph[0] == 0, "dark glyph on an opaque light tile is preserved");
    glyph.rgba.assign(8 * 8 * 4, 0); for (int i = 0; i < 64; ++i) glyph.rgba[i * 4 + 3] = 255;
    for (int c = 0; c < 3; ++c) glyph.rgba[9 * 4 + c] = 255;
    PrepareMapIconGlyph(glyph);
    Check(glyph.glyph[9] == 1 && glyph.glyph[0] == 0, "light glyph on an opaque dark tile is preserved");
    for (int y = 1; y < 7; ++y) for (int x = 1; x < 7; ++x)
        for (int c = 0; c < 3; ++c) glyph.rgba[(y * 8 + x) * 4 + c] = 255;
    PrepareMapIconGlyph(glyph);
    Check(glyph.glyph[9] == 1 && glyph.glyph[0] == 0, "large light glyph is not mistaken for the tile background");
    g_worldMapIconStyle = true;
    Check(TryDrawMinimapRasterIcon(renderer, nullptr, 64, 64, 40, 100, 64, iconKey, true), "actual atlas icon draws in the world-map style");
    bool outside = false;
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x)
        if ((x-64)*(x-64) + (y-64)*(y-64) > 40*40 && pixels[y][x].r != 0) outside = true;
    Check(!outside, "gold diamond and glyph stay clipped to the circular map");
    g_worldMapIconStyle = false;
    Check(TryDrawMinimapRasterIcon(renderer, nullptr, 64, 64, 40, 64, 64, iconKey, false), "original icon style remains available");
    if (argc > 1) CheckClientLayout(argv[1]);
    if (argc > 2)
    {
        std::memset(pixels, 0, sizeof(pixels));
        g_worldMapIconStyle = true;
        Check(TryLoadRealMapFromPath("assets/embervale_realmap_1024.rgba", g_realMap), "load real terrain for CPU preview");
        g_realMap.attempted = true;
        DrawRealMap(renderer, nullptr, 160, 160, 130, 2200, 2200, 2.0f, 0);
        for (int i = 0; i < 5; ++i)
            CmdClearPoiIcon(renderer, nullptr, 160, 160, 130, 95 + i * 33, 130, (nativeIcon + i)->key, false);
        CmdClearPoiIcon(renderer, nullptr, 160, 160, 130, 120, 195, 11, false);
        CmdClearPoiIcon(renderer, nullptr, 160, 160, 130, 195, 195, 12, false);
        DrawWaypointDiamond(renderer, nullptr, 160, 160); DrawPingDiamond(renderer, nullptr, 195, 90);
        DrawDeathMarker(renderer, nullptr, 160, 160, 130, 85, 185, 0x1AD8F96E);
        WritePreview(argv[2]);
    }
    return 0;
}
