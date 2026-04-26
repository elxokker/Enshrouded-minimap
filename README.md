# Embervale Minimap

Integrated in-game minimap mod for Enshrouded running through Shroudtopia.

This project is intentionally standalone and clean: it contains only the minimap
source, the minimal Shroudtopia headers needed to compile it, the runtime assets,
and build/install scripts. It does not include the full Shroudtopia checkout,
temporary dumps, scratch data, backups, or compiled output.

## Current State

- Native in-frame Vulkan minimap, no external overlay window.
- Premium circular frame asset and real Embervale map texture.
- Player arrow asset.
- Real icon atlas extracted from the game map UI.
- Fog-of-war/static POI fallback.
- Live hooks for player position, waypoint UI, nearby markers, render present,
  Vulkan device table, and map-marker visibility.
- Current mod version: `0.4.34`.

## Layout

- `src/` - mod source and precompiled-header files.
- `include/` - minimal Shroudtopia SDK headers used by the mod.
- `assets/` - files copied next to `minimap_mod.dll` in the game mod folder.
- `build-release.ps1` - Release x64 build helper.
- `install.ps1` - copies the DLL and assets into the Enshrouded mod folder and
  backs up the previous install.

## Build

Run from this folder:

```powershell
.\build-release.ps1
```

The script uses Visual Studio/MSBuild and defaults to `Release|x64`.

## Install

Run:

```powershell
.\install.ps1
```

By default it installs to:

`C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\mods\minimap_mod`

Pass `-GameDir` if Enshrouded is installed somewhere else.

## Notes

This is client-side mod code for a specific Enshrouded/Shroudtopia build. If the
game updates, hook RVAs and structure offsets may need to be revalidated.
