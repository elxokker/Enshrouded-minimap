# Death markers

This branch adds tombstones to the minimap using the existing completed-frame
world-map capture. It adds no game hook and no server component.
`mods.minimap_mod.show_death_markers` defaults to `true` and refreshes every
second. The marker uses its atlas icon if available; the v0.5.0 atlas has no
tombstone key, so the renderer draws a compact cream skull with dark eyes.

Deaths use their own layer, independent of POI deduplication, `max_icons` and
`show_world_markers`. They rotate and zoom with the map and remain at its rim
when distant. The selected waypoint outline draws over the skull.

## Verified Steam layout

The installed client data revision is 1076226.

| Evidence | Location |
| --- | --- |
| Existing map hook | `clear_map_markers_for_ui`, RVA `0x29DE20` |
| Death-list clear | RVA `0x29DE49` zeros UI-state count `+0x7C180` |
| Death-list selection | RVA `0x2A9D8F` selects `+0x7C178` in `replicate_map_markers_for_ui` |
| Marker allocation | Call at `0x2A9DB4` to `0x29C3C0`; stride shift at `0x29C3FA` |
| Entry layout | 0x80 bytes; signed 32.32 xyz at +0x10/+0x18/+0x20, icon key +0x30 |
| Full-map consumer | List read at `0xB00DD0`; position conversion at `0xB01000` |
| Observed death icon key | `0x1AD8F96E` |

The death layout is checked separately before installing the existing map hook.
A mismatch disables only death capture; the existing POI reader remains usable.
Up to 1024 death entries are copied at most ten times per second, bounded to
the game's initial list capacity. The renderer receives owned values only.
Invalid pointers or counts clear deaths without clearing other map markers.
An empty list removes recovered graves, interrupted snapshots expire after
two seconds, and world exit clears the snapshot.

On 2026-10-04, a read-only query of the running game found one tombstone with
valid world coordinates and key `0x1AD8F96E`. The player identified the visible
full-map tombstone as a friend's. This confirms that their client supplies such
a marker; it does not establish visibility for every player, distance or world.
No process memory was modified by that query.

## Validation and gameplay acceptance

Release/x64 builds with MSVC v143 passed all 187 focused checks: 17 tracking,
27 renderer, 58 live-marker and 85 map-feature checks, including the installed
executable layout check. The test DLL SHA256 is
`71D850560A8F4C3B139ABF9CFBFCF6610EDF16AA9652ABC7B68E001F51484D9F`.
On 2026-10-04, the player confirmed that the installed build shows the tombstone and that its marker disappears after retrieval. The visible tombstone used for testing belonged to a friend. This confirms display and removal for that scenario.

The production-reader tests cover multiple graves, position updates, invalid
coordinates, recovery/removal, a later death, stale snapshots and tick wrap,
unreadable/oversized lists, world exit, the runtime visibility toggle, and
failure isolation from POIs. CPU drawing checks exercise the actual live-marker
path, skull eyes, zoom/rotation and circular map bounds. The read-only PE test
checks the installed executable and rejects mutations to each new layout gate.

For regression testing, compare a grave with the full map, walk and turn, change zoom,
select it as a waypoint, and confirm it disappears after recovery. Recheck both
your own and a friend's tombstone, world re-entry and first-/third-person views.
Instanced dungeon deaths and the full map's dungeon-entrance aggregation have
not been validated by this change. This build copies map-space tombstone
coordinates; it does not add dungeon-entrance lookup.
