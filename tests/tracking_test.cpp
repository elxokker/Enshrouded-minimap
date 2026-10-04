// Include the implementation so these tests exercise the production reader.
#include "../src/dllmain.cpp"
#include <iostream>
#include <limits>

namespace
{
    struct TestTransform
    {
        std::int64_t position[3];
        float quaternion[4];
    } playerTransform, cameraTransform;

    std::array<unsigned char, 0x800> testDescriptor{};
    std::uint32_t localPlayerData[2] = { 0, 9 };
    std::uint32_t entityIds[8] = { 7, 0, 0, 0, 9, 0, 0, 0 };
    int nextCalls = 0;

    void __fastcall TestInit(void* ctx, void* buffer, std::uint32_t)
    {
        auto* record = static_cast<uintptr_t*>(buffer);
        record[0] = reinterpret_cast<uintptr_t>(ctx);
        // Initialization can leave non-null placeholders before the first entity.
        record[2] = 1;
        record[6] = 1;
    }

    bool __fastcall TestNext(void* ctx, void* buffer, std::uint32_t)
    {
        ++nextCalls;
        auto& index = *reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(ctx) + 8);
        if (index >= 2)
            return false;
        ++index;
        auto* record = static_cast<uintptr_t*>(buffer);
        record[2] = reinterpret_cast<uintptr_t>(&playerTransform);
        record[3] = reinterpret_cast<uintptr_t>(localPlayerData);
        record[6] = reinterpret_cast<uintptr_t>(&cameraTransform);
        return true;
    }

    void Check(bool ok, const char* description)
    {
        if (!ok)
        {
            std::cerr << "FAIL: " << description << '\n';
            std::exit(1);
        }
        std::cout << "PASS: " << description << '\n';
    }

    bool PublishTransforms()
    {
        return TryPublishLocalCameraTransform(reinterpret_cast<uintptr_t>(&playerTransform),
            reinterpret_cast<uintptr_t>(&cameraTransform));
    }
}

int main()
{
    unsigned char image[0x400] = {};
    g_exeBase = reinterpret_cast<uintptr_t>(image);
    g_exeImageSize = sizeof(image);
    const char systemName[] = "player_camera_transform";
    std::memcpy(image + 0x100, systemName, sizeof(systemName));
    uintptr_t descriptor[3] = { g_exeBase + 0x100, sizeof(systemName) - 1, g_exeBase + 0x300 };
    std::memcpy(image + 0x40, descriptor, sizeof(descriptor));
    Check(IsNamedSystemAt(0x40, systemName, 0x300), "resolve named registry using length excluding NUL");
    Check(!IsNamedSystemAt(0x40, systemName, 0x310), "reject wrong function with matching name");
    Check(!IsNamedSystemAt(0x40, "player_waypoints_ui", 0x300), "reject wrong system name");

    g_directCameraTrackingSupported = true;
    g_iterInit = TestInit;
    g_iterNext = TestNext;
    const auto ids = reinterpret_cast<uintptr_t>(entityIds);
    std::memcpy(testDescriptor.data() + 0x7E0, &ids, sizeof(ids));
    std::uint64_t context[2] = { reinterpret_cast<uintptr_t>(testDescriptor.data()), 0 };
    playerTransform = { { WorldToFixed(3810), WorldToFixed(825), WorldToFixed(1898) }, { 0, 0, 0, 1 } };
    cameraTransform = playerTransform;
    CapturePlayerCameraTransformHook(context, nullptr, nullptr, nullptr);
    Check(g_playerPosition.valid, "capture through camera hook");
    Check(context[1] == 0, "leave game iterator cursor untouched");
    Check(nextCalls == 2, "advance despite init placeholders and skip remote entity");
    Check(g_playerPosition.channel == 30 && g_playerPosition.x == playerTransform.position[0] &&
        g_playerPosition.hasHeading && std::abs(g_playerPosition.headingRadians) < 0.0001f,
        "publish local transform and heading");

    playerTransform.position[0] = WorldToFixed(3900);
    playerTransform.position[2] = WorldToFixed(1920);
    cameraTransform = playerTransform;
    cameraTransform.position[0] += WorldToFixed(5);
    cameraTransform.quaternion[1] = 0.70710678f;
    cameraTransform.quaternion[3] = 0.70710678f;
    CapturePlayerCameraTransformHook(context, nullptr, nullptr, nullptr);
    Check(g_playerPosition.x == playerTransform.position[0] &&
        g_playerPosition.z == playerTransform.position[2] &&
        std::abs(g_playerPosition.headingRadians - 1.5707963f) < 0.0001f,
        "update player center and camera heading independently");

    const auto good = g_playerPosition;
    auto stale = good;
    stale.x = WorldToFixed(100);
    stale.channel = 5;
    stale.hasHeading = false;
    PublishPlayerPosition(stale);
    stale.channel = 24;
    stale.hasHeading = true;
    PublishPlayerPosition(stale);
    Check(g_playerPosition.x == good.x && g_playerPosition.headingRadians == good.headingRadians,
        "prevent static anchor and render camera from overriding local player");

    cameraTransform.quaternion[1] = std::numeric_limits<float>::quiet_NaN();
    Check(!PublishTransforms(), "reject non-finite rotation");
    cameraTransform.quaternion[1] = 0;
    cameraTransform.quaternion[3] = 0;
    Check(!PublishTransforms(), "reject zero quaternion");
    cameraTransform.quaternion[3] = 1;
    playerTransform.position[0] = WorldToFixed(50000);
    Check(!PublishTransforms(), "reject off-map position");
    playerTransform.position[0] = WorldToFixed(3900);
    cameraTransform.position[0] = WorldToFixed(4500);
    Check(!PublishTransforms(), "reject unrelated camera far from player");
    Check(!TryPublishLocalCameraTransform(0, reinterpret_cast<uintptr_t>(&cameraTransform)),
        "reject unreadable transform");

    uintptr_t record[10] = {};
    Check(!TryReadLocalCameraRecord(nullptr, record), "reject absent iterator context");
    g_iterNext = nullptr;
    Check(!TryReadLocalCameraRecord(context, record), "reject unavailable iterator function");
    g_directCameraTrackingSupported = false;
    PublishPlayerPosition(stale);
    Check(g_playerPosition.x == stale.x && g_playerPosition.channel == stale.channel,
        "retain legacy position publishing when named camera tracking is unavailable");
    return 0;
}
