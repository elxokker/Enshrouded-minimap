# World-map display and graphics extensions

These changes bring the remaining user-facing features and graphics fixes from
[PR #4](https://github.com/elxokker/Enshrouded-minimap/pull/4) into PRs #5 and #6
for Steam client data revision 1076226. PR #4's marker layout findings and visual
design were the starting point; the data-reader layout was checked independently
against the installed Steam executable before use.

| Capability | Implementation |
| --- | --- |
| Movement, rotation and local-player selection | PR #5's named `player_camera_transform` reader, retained |
| Safe command-buffer reuse | PR #5's nonblocking submission fences, retained |
| Recreated graphics surfaces | PR #5 checks image handles and creation generations; recording/submission access violations stop overlay drawing |
| World-map icons, altars and NPC markers | PR #6 copies the completed main world-map list; native atlas keys where available, dedicated altar/NPC fallbacks otherwise |
| Custom map pins | PR #6 copies the custom-pin list and displays red flags |
| Other players and pings | Existing green player arrows and ping diamonds, retained |
| Selected waypoint | Existing hollow yellow outline, retained instead of replacing the underlying icon with a flag |
| Brighter terrain and icon styling | Configurable parchment lighting, original atlas icons by default, optional gold diamond frames and dark glyphs |
| Zoom and rotation polish | Zoom steps -3 through +7; configurable frame-time heading smoothing |
| Map-edge clutter | Ordinary distant POIs are omitted; custom pins, NPCs, players, pings and the selected waypoint can remain at the edge |
| Diagnostics | Existing tracking/render diagnostics plus throttled snapshot counts and unknown atlas keys behind `debug_logging` |

The current Steam camera reader replaces the purpose of PR #4's background
camera search, position fusion and remote-camera rejection heuristics. Those
scanners are not added alongside the tested reader. Compatibility with the
different executable variant reported in PR #4 is not established by these
changes; this is feature coverage for the verified Steam layout, not a claim
that every client version is supported.

## Live display settings

These settings belong under `mods.minimap_mod` and refresh every second:

| Setting | Default | Behavior |
| --- | --- | --- |
| `show_world_markers` | `true` | Use the world-map and custom-pin snapshots; `false` restores the previous POI path |
| `map_light` | `55` | Integer 0-100; 0 restores the original dark terrain treatment |
| `icon_style` | `"original"` | Source atlas icons and colors; `"world-map"` enables gold frames and dark glyphs |
| `heading_smoothing_ms` | `55` | Integer 0-250; 0 follows the camera immediately |

The existing `show_other_players`, `show_pings`, and `show_waypoints` settings
continue to control their independent live feeds. `show_waypoints` controls the
selected destination; permanent custom pins belong to `show_world_markers`.
The POI `max_icons` limit still applies. Glyph masks are prepared when the atlas
loads, rather than recalculated for every icon on every frame.

## Steam layout and capture timing

| Evidence | Verified location |
| --- | --- |
| `clear_map_markers_for_ui` | Registry RVA `0x1D45390`, entry `0x29DE20` |
| Singleton UI-state record | 0x10 bytes, state pointer at +0x08; iterator initializer `0x8DA7C0` |
| Main map array | UI state +0x1C100, count +0x1C108 |
| Custom map array | UI state +0x3C128, count +0x3C130 |
| Entry layout | 0x80-byte stride; signed 32.32 xyz at +0x10, icon key at +0x30 |
| Custom pin writer | Registry `0x1D47820`, entry `0x2A0C30`; stride at `0x2A0D34` |
| Main marker writer | Registry `0x1D4C100`, entry `0x2A9C50`; position writes at `0x2A9FA3`, key write at `0x2AA0E9` |

The entry hook copies the preceding completed frame before the game clears and
rebuilds these lists. The private iterator copy preserves the game's cursor.
Each list is capped at 2048 entries, read at most ten times per second, checked
for valid coordinates, and published as owned values. Empty or unreadable lists
remove previous entries; interrupted snapshots expire after two seconds and
world exit clears them. Dedicated ping capture prevents mirrored ping duplicates.
Custom pins take priority over coincident POIs. While the verified mirror is
enabled, stale static/visibility caches do not repopulate an empty list.

## Verification

Build `tests/map_features_test.vcxproj` in Release/x64 and run from the repo root.
As with the other projects, `/p:PlatformToolset=v143` selects Visual Studio 2022.
Two optional arguments add a read-only layout check and a CPU-rendered preview:

```powershell
.\build\tests\map_features_test.exe '<game directory>\enshrouded.exe' '<output directory>\map-preview.bmp'
```

The test exercises production capture, empty/invalid/stale data, private iterator
state, custom-pin priority, live configuration, lighting, heading wrap/reset,
actual atlas glyphs and circle clipping. The optional executable check maps bytes
into a local test buffer; it neither starts nor modifies the game. A modified
stride or clear-array instruction must fail the production layout gate.

On 2026-10-04, the user confirmed the deployed combined build from
`0ab0ffd2a5a28a044b8cbcc5ca76c461979e429f` was working and requested both PRs be
marked ready for review. The tested DLL SHA256 is
`A20012F47AE3C3D082C9760186BC78578DC40820503A21C6DD95B6A9C5C0A8CA`.
The confirmation followed selecting `icon_style: "original"`; that setting is
now the default in code and the manifest, with gold frames remaining opt-in.
The change makes the reported configuration the default without changing either
renderer style. The acceptance applies to that deployed build and configuration.

For future regression testing, compare altars, NPCs and custom pins against the
full map; add/remove a pin; recheck players, pings and the selected-icon waypoint;
exercise all zoom levels and live display options; then test world re-entry,
resolution changes, alt-tab/display sleep and an extended session. Individual
results for those scenarios were not separately recorded. Automated checks and
CPU previews do not establish real-GPU stability or an FPS improvement.
