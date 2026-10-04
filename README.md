# Enshrouded Minimap

Integrated in-game minimap mod for Enshrouded using Shroudtopia.

This is not an external Windows overlay. The mod hooks into the game client and
draws the minimap directly inside the Enshrouded frame.

## What It Does

- Shows a circular minimap on the right side of the screen.
- Defaults to the bottom-right corner and supports top-right, middle-right,
  and bottom-right placement.
- Uses a premium compass-style frame asset.
- Renders the real Embervale map at minimap scale.
- Shows the player's position and facing direction.
- Uses real map marker icons extracted from the game's map UI.
- Shows nearby points of interest that are visible or detected by the map.
- Shows other players as green arrows, pings as green diamonds, and your
  selected waypoint as a hollow yellow outline.
- Mirrors world-map locations, altars, NPC markers and custom pins.
- Development build: shows tombstones as skull markers, including friends'
  tombstones supplied by the client. See [death marker testing](docs/death-markers.md).
- Uses original location icons by default, with optional gold frames,
  adjustable terrain lighting and heading smoothing.
- Uses fog-of-war and POI data as a fallback when the game does not expose all
  live markers.
- Renders inside the game's Vulkan frame without a separate overlay window.

## Controls

- `+` zooms in.
- `-` zooms out.
- `F10` toggles the minimap on/off without leaving the game.

The numpad `+`, `-`, and `*` keys also work.

## Download

Download the latest zip from the release page:

[Enshrouded Minimap v0.4.46](https://github.com/elxokker/Enshrouded-minimap/releases/tag/v0.4.46)

Release asset:

`enshrouded-minimap-v0.4.46.zip`

## Installing the Zip

1. Close Enshrouded.
2. Download `enshrouded-minimap-v0.4.46.zip`.
3. Extract the zip.
4. Copy the full `minimap_mod` folder to:

```text
C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\mods\minimap_mod
```

The final folder should look like this:

```text
Enshrouded
+-- mods
    +-- minimap_mod
        +-- minimap_mod.dll
        +-- mod.json
        +-- embervale_minimap_frame.rgba
        +-- embervale_minimap_icons.bin
        +-- embervale_player_arrow.rgba
        +-- embervale_realmap_1280.rgba
        +-- ...
```

5. Start Enshrouded with Shroudtopia.

If you already had an older version installed, replace the whole `minimap_mod`
folder with the new one.

## Position Config

The minimap position is controlled from Shroudtopia's config file:

```text
C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\shroudtopia.json
```

Set `mods.minimap_mod.position` to one of these values:

- `top-right`
- `middle-right`
- `bottom-right`

Example:

```json
{
  "mods": {
    "minimap_mod": {
      "active": true,
      "position": "bottom-right",
      "toggle_key": "F10",
      "render_camera_fallback": true,
      "debug_logging": false,
      "show_other_players": true,
      "show_pings": true,
      "show_waypoints": true,
      "show_world_markers": true,
      "show_death_markers": true,
      "map_light": 55,
      "icon_style": "original",
      "heading_smoothing_ms": 55,
      "map_sample_step": 2,
      "max_icons": 64
    }
  }
}
```

The mod also reads `mods.minimap_mod.toggle_key` every second while active.
Recommended value: `F10`. Supported readable values include `F1`-`F24`,
`insert`, `delete`, `home`, `end`, `pageup`, `pagedown`, `backspace`, and
`numpad-*`.

`render_camera_fallback` is enabled by default. It lets the minimap recover its
player position from render data when a game update stops the normal UI position
feed from firing before the minimap draws.

The mod reads these values directly and refreshes them every second while active.
If the minimap is not loaded yet, start or restart the game after changing it.

## Live markers and world-map display

The `feature/multiplayer-player-markers` branch adds client-side markers for
Steam client data revision 1076226. These changes are not in the release zip above.
Other-player markers, pings and waypoints were confirmed working in user
gameplay testing on 2026-10-03.
The combined world-map display and graphics build was confirmed working in
user gameplay testing on 2026-10-04, with original location icons selected.
Both PRs are ready for review following that confirmation; individual
extended-session and display-change results were not separately recorded.

- Other players use the local player's arrow artwork tinted green, sampled up
  to ten times per second. Arrows follow their facing direction relative to the map.
  Only players whose transforms are available on your client can be shown;
  full-world coverage is not guaranteed. Missing transforms remove the marker,
  and an interrupted feed expires after two seconds.
- Your pings and other players' pings appear as green diamonds with a bright
  border and a dark downward arrow. A new ping replaces that sender's old ping;
  pings expire eight seconds after their last event.
- Active waypoints appear as hollow yellow diamonds. Their centres remain
  transparent so the map and any POI icon underneath stay visible. Moving or
  clearing a waypoint replaces or removes its marker.

Markers rotate and zoom with the map, and positions beyond the minimap edge
are clamped to its rim. They have separate visibility settings:
`show_other_players`, `show_pings`, and `show_waypoints` (all default to `true`).
These settings refresh every second. Live markers do not consume the POI
`max_icons` allowance. No server component is added by this feature.

In the death-marker development build, `show_death_markers` also defaults to
`true` and refreshes live. Tombstones have their own layer, so hiding ordinary
POIs or reaching `max_icons` does not hide them. Distant tombstones stay at the
rim; recovering one removes its marker when the game removes it from the map
list. The existing icon atlas uses a small cream skull fallback. This feature
is not included in the v0.4.46 download linked above.

See [live marker validation](docs/live-markers.md) for layout evidence and the
in-game regression checklist.

The branch also mirrors world-map POIs, altars, NPC markers and custom pins,
adds parchment lighting and optional gold-framed icons, supports zoom up to +7, and
offers configurable rotation smoothing. Ordinary distant POIs no longer crowd
the map edge. These additions retain the green player/ping markers and the
hollow yellow selected-waypoint outline. Location icons use the original atlas
style by default; set `icon_style` to `"world-map"` to enable gold frames.

See [display settings and PR #4 feature coverage](docs/pr4-feature-parity.md)
for `map_light`, `icon_style`, `heading_smoothing_ms`, `show_world_markers`,
compatibility boundaries and test instructions.

## EML Compatibility Notes

This minimap is a native Shroudtopia mod. It is not an EML-native Lua mod and
EML does not load `minimap_mod.dll` by itself.

The release includes a no-op `src/mod.lua` and EML-neutral manifest fields so
EML setups that scan every folder under `mods/` do not treat `minimap_mod` as an
incomplete Lua package.

For mixed EML + Shroudtopia setups:

- Keep Shroudtopia installed with `winmm.dll` and `shroudtopia.dll`.
- Keep this mod at `mods/minimap_mod`.
- If EML with `dbghelp.dll` crashes on client startup, try EML's `dinput8.dll`
  proxy instead of `dbghelp.dll`, as recommended by EML's own troubleshooting.
- Do not expect an EML-only setup to load the minimap DLL. Shroudtopia is still
  required.

## Building From Source

Requirements:

- Windows.
- Visual Studio with MSBuild and the C++ toolset.
- Shroudtopia installed in the game folder to load the mod.

Build:

```powershell
.\build-release.ps1
```

Install to the default Steam path:

```powershell
.\install.ps1
```

Install to a custom Enshrouded path:

```powershell
.\install.ps1 -GameDir "D:\SteamLibrary\steamapps\common\Enshrouded"
```

## Repository Layout

- `src/` - mod source code.
- `include/` - minimal Shroudtopia headers required to build.
- `assets/` - runtime assets copied next to the DLL.
- `build-release.ps1` - builds `Release|x64`.
- `install.ps1` - installs the mod and backs up the previous install.

## Version

Current mod version: `0.4.46`.

The mod resolves hook signatures near the known Enshrouded client addresses at
load time, which makes small game updates less likely to break the minimap.
Large game updates can still require structure offsets to be revalidated.
