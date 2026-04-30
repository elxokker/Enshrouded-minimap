Enshrouded Minimap v0.4.44
==========================

Integrated in-game minimap mod for Enshrouded using Shroudtopia.

This is not an external overlay. The mod draws the minimap directly inside the
game frame.

What it does
------------

- Shows a circular minimap on the right side of the screen.
- Defaults to the bottom-right corner.
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
- F10 key: toggle minimap on/off without leaving the game.

The numpad +, -, and * keys also work.

Zip installation
----------------

1. Close Enshrouded.
2. Extract enshrouded-minimap-v0.4.44.zip.
3. Copy the full minimap_mod folder to:

   C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\mods\minimap_mod

4. The folder must contain minimap_mod.dll, mod.json, and the .rgba/.bin files.
5. Start Enshrouded with Shroudtopia.

If you already had an older version installed, replace the whole minimap_mod
folder with the new one.

Position config
---------------

The minimap position is controlled from Shroudtopia's config file:

   C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\shroudtopia.json

Set mods.minimap_mod.position to one of these values:

- top-right
- middle-right
- bottom-right

Example:

   "minimap_mod": {
     "active": true,
     "position": "bottom-right",
     "toggle_key": "F10",
     "render_camera_fallback": false,
     "debug_logging": false,
     "map_sample_step": 2,
     "max_icons": 64
   }

The mod also reads mods.minimap_mod.toggle_key every second while active.
Recommended value: F10. Supported readable values include F1-F24, insert,
delete, home, end, pageup, pagedown, backspace, and numpad-*.

The mod reads these values directly and refreshes them every second while active.
If the minimap is not loaded yet, start or restart the game after changing it.
