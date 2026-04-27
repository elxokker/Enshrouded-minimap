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
- Uses fog-of-war and POI data as a fallback when the game does not expose all
  live markers.
- Renders inside the game's Vulkan frame without a separate overlay window.

## Controls

- `+` zooms in.
- `-` zooms out.

The numpad `+` and `-` keys also work.

## Download

Download the latest zip from the release page:

[Enshrouded Minimap v0.4.36](https://github.com/elxokker/Enshrouded-minimap/releases/tag/v0.4.36)

Release asset:

`enshrouded-minimap-v0.4.36.zip`

## Installing the Zip

1. Close Enshrouded.
2. Download `enshrouded-minimap-v0.4.36.zip`.
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
      "position": "bottom-right"
    }
  }
}
```

The mod reads this value directly and refreshes it every second while active.
If the minimap is not loaded yet, start or restart the game after changing it.

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

Current mod version: `0.4.36`.

This mod targets a specific Enshrouded/Shroudtopia build. If the game updates,
hook RVAs and structure offsets may need to be revalidated.
