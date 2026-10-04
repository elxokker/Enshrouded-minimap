#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace MinimapLive
{
    constexpr std::uint32_t PlayerStaleMs = 2000;
    constexpr std::uint32_t PingLifetimeMs = 8000;
    constexpr std::size_t MaxPlayers = 64;
    constexpr std::size_t MaxPings = 64;

    // Own values only. No game-memory pointers survive the capture callback.
    struct Marker
    {
        std::uint32_t id = 0;
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        std::uint32_t updated = 0;
        float heading = 0.0f;
    };

    inline bool Fresh(const Marker& marker, std::uint32_t now, std::uint32_t lifetime)
    {
        // Unsigned subtraction also handles GetTickCount wrapping.
        return now - marker.updated < lifetime;
    }

    inline void Prune(std::vector<Marker>& markers, std::uint32_t now, std::uint32_t lifetime)
    {
        markers.erase(std::remove_if(markers.begin(), markers.end(), [=](const Marker& marker)
        {
            return !Fresh(marker, now, lifetime);
        }), markers.end());
    }

    inline void UpsertPing(std::vector<Marker>& markers, const Marker& ping)
    {
        // A new ping from the same sender replaces their previous ping.
        auto existing = std::find_if(markers.begin(), markers.end(), [&](const Marker& marker)
        {
            return marker.id == ping.id;
        });
        if (existing != markers.end())
            *existing = ping;
        else
        {
            if (markers.size() == MaxPings)
                markers.erase(markers.begin());
            markers.push_back(ping);
        }
    }
}
