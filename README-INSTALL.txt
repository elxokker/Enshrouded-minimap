Enshrouded Minimap v0.4.34
==========================

Integrated in-game minimap mod for Enshrouded using Shroudtopia.

This is not an external overlay. The mod draws the minimap directly inside the
game frame.

What it does
------------

- Shows a circular minimap in the top-right corner.
- Uses the real Embervale map at minimap scale.
- Shows the player's position and facing direction.
- Uses real map marker icons extracted from the game's map UI.
- Shows nearby points of interest visible or detected by the map.
- Uses fog-of-war and POI data as a fallback when the game does not expose all
  live markers.

Controls
--------

- + key: zoom in.
- - key: zoom out.

The numpad + and - keys also work.

Zip installation
----------------

1. Close Enshrouded.
2. Extract enshrouded-minimap-v0.4.34.zip.
3. Copy the full minimap_mod folder to:

   C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\mods\minimap_mod

4. The folder must contain minimap_mod.dll, mod.json, and the .rgba/.bin files.
5. Start Enshrouded with Shroudtopia.

If you already had an older version installed, replace the whole minimap_mod
folder with the new one.
