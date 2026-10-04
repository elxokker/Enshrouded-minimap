# Live minimap markers

This feature is on `feature/multiplayer-player-markers`, based on the
camera and Vulkan fixes in PR #5. It adds client-only capture and drawing;
no server component or network protocol is changed. Other-player markers,
pings and waypoints were confirmed working in user gameplay testing on
2026-10-03.

The subsequent world-map display and graphics extensions are described in
[PR #4 feature coverage](pr4-feature-parity.md). The user confirmed the combined
build was working on 2026-10-04 after selecting the original location-icon style,
and requested that both PRs be marked ready for review.

## Display and configuration

- Players: the same arrow asset as the local player, tinted green and rotated
  by the remote transform's facing direction relative to the minimap heading.
  Position and rotation are sampled at most every 100 ms.
- Pings: green diamond, bright rim, dark downward arrow. Both local inputs and
  received ping events are supported. One current ping per sender, expiring
  eight seconds after its last event.
- Waypoints: hollow yellow diamond with a transparent centre. The outline
  does not clear or cover the map/POI icon beneath it.

`mods.minimap_mod.show_other_players`, `show_pings`, and `show_waypoints`
default to `true` and refresh with the existing one-second config polling.
Live markers are independent of POI visibility filtering and `max_icons`.
Off-map-screen positions clamp to the minimap rim. No player names are drawn.

Remote markers require a transform in the local ECS world. This is not proof
that every connected player's position is available across the entire world.
Missing transforms remove their markers; capture interruption expires players
and waypoints after two seconds. Session close and mod deactivation clear all
three feeds. Rendering only receives copied values, never ECS pointers.

## Verified layout for client data revision 1076226

The installed Windows client was inspected statically. These are image RVAs,
not process addresses. Registry names and relevant code bytes gate the new path.
An unsupported layout leaves the new player hook disabled.

| Source | Verified layout |
| --- | --- |
| `update_ui_data` | Registry `0x1D50090`, entry `0x2AF690`, singleton record size `0x98` |
| Player record | `+0x08` BasicPlayerInfos, `+0x10` LocalPlayerData, `+0x30` RenderTransform lookup |
| BasicPlayerInfos | 1023 slots, `0x60` stride, entity ID at slot start; ID must match slot index + 1 |
| LocalPlayerData | Local entity ID at `+0x04`, excluded from remote markers |
| Transform lookup | World helper `0x8D3630`, component lookup `0x8C72A0`, two-word output |
| RenderTransform | Three signed 32.32 coordinates at `+0x00`; quaternion at `+0x18` |
| `player_waypoints_ui` | Registry `0x1D4B0B0`, entry `0x2A8370`, singleton record size `0x30` |
| Ping lists | Record `+0x20` UiPingEvent, `+0x28` UiPingInputEvent; entries at list `+0x18`, count at `+0x28` |
| Ping entry | `0x28` stride, sender ID `+0x08`, three signed 32.32 coordinates `+0x10` |
| Local waypoint | Record `+0x18` PlayerWaypoint lookup, local ID from LocalPlayerData; enabled byte `+0x00`, three signed 32.32 coordinates `+0x08` |

The old `playerList` name referred to UiPingInputEvent, not player locations.
The new path does not interpret ping events or copied ping UI state as players
or generic POIs. It uses the declared RenderTransform lookup on the ECS callback
thread. The existing camera reader and Vulkan submission ordering are retained.

The first in-game test confirmed pings worked but selected-icon waypoints were
missing. The original capture used the UI player array at state `+0x305550`.
The game rebuilds and zeroes those entries around `0x2690C5` through `0x26914C`,
then `player_waypoints_ui` fills their waypoint fields. An entry hook therefore
sees the cleared active flag. The reader now uses the local PlayerWaypoint
component directly, through this system's declared lookup. Clearing its enabled
byte removes the marker even if the UI still holds the previous coordinates.

## Focused checks

Build `tests/markers_test.vcxproj`, `tests/tracking_test.vcxproj`, and
`tests/renderer_test.vcxproj` in Release/x64 using the installed C++ toolset.
Run from the repository root so the marker test can load the real arrow asset.
The marker executable accepts an optional BMP output path to inspect the
production drawing commands on a CPU target.

Marker checks cover local-player exclusion, separate identities at the same
position, movement and facing updates, missing/invalid transforms, private
iterator state, bounded pings, expiry including tick wrap, moved/cleared
waypoints, config visibility, session cleanup, projection/rotation and edge
clamping, plus the requested icon colours and transparent waypoint centre.
The waypoint regression keeps the UI active flag cleared while the component is
enabled, reproducing the timing that the initial synthetic fixture missed.
Camera and renderer regression tests remain separate.

## In-game validation

On 2026-10-03, the user confirmed that multiplayer markers, pings and waypoints
had been tested and were working. This includes the revised waypoint reader.
On 2026-10-04, the deployed combined build from `0ab0ffd` was also reported
working with `icon_style: "original"`. That style is now the default, with gold
frames remaining opt-in. Extended-session stability has not been separately
confirmed. The following checklist is retained for regression testing; these
confirmations do not record individual results for every scenario below.

1. Join a world with a friend. Walk and turn independently; compare their green
   arrow with their position on the full map. Check nearby, far away, after fast
   travel, after death/respawn, and after disconnect/rejoin. Establish how far
   the client supplies remote transforms and whether facing matches the model.
2. Each player places a ping. Check the green diamond follows the map's rotation
   and zoom, a replacement moves it, and the old ping disappears after expiry.
3. Place a waypoint over an existing map icon. Check that the icon remains
   visible inside the yellow outline; move and clear it. Repeat on empty terrain.
4. Toggle each visibility setting independently. Verify other marker types and
   the local-player arrow remain visible. Leave/rejoin the world and verify
   markers from the previous session do not persist.
5. Compare performance and complete an extended multiplayer session, including
   alt-tab/display sleep and return. Synthetic checks do not prove live stability.

Install or roll back a test DLL only with Enshrouded fully closed. Keep the tested
DLL available for rollback. PR #5 contains the shared camera and Vulkan fixes;
the marker and display additions are in PR #6.
