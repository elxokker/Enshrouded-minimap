# Camera tracking regression tests

From a Visual Studio x64 Native Tools prompt at the repository root:

```powershell
MSBuild tests\tracking_test.vcxproj /p:Configuration=Release /p:Platform=x64
.\build\tests\tracking_test.exe
```

The project defaults to v142, matching the mod. For Visual Studio 2022 with
v143 installed, add `/p:PlatformToolset=v143` to the build command. Build output
can be redirected with the usual `OutDir` and `IntDir` MSBuild properties.

The executable includes the production reader and supplies synthetic ECS records.
It exits with a nonzero status on failure, including in Release builds. It checks:

- Named system identity and the registry's string length convention.
- Advancing the copied iterator, filtering the local entity, and preserving the
  original cursor.
- Updating player position and camera heading independently.
- Rejecting stale fallback samples, invalid transforms, and unavailable iterators.
- Preserving legacy publication when the named camera path is unavailable.

These tests do not load or hook the game and do not validate Vulkan rendering.

## Client 1076226 layout

The new path is limited to the observed Steam client layout. It requires matching
registry names and function pointers for `player_camera_transform` and
`player_waypoints_ui`, the expected iterator targets, and the camera entry bytes.
All addresses in the implementation are image-relative RVAs.

The camera iterator's 80-byte record provides `RenderTransform` at +0x10,
`LocalPlayerData` at +0x18, and `LocalPlayerCameraTransform` at +0x30. Position uses
the player's 32.32 fixed-point coordinates; heading uses the camera quaternion.
The local entity is selected by comparing the iterator's current entity ID with
`LocalPlayerData` at +4, matching the game's camera system. The iterator advances
its context at +8, so the hook operates on a private 16-byte context copy.

The same client moves the waypoint array to +0x305550. Its named registry entry
avoids a different system with an identical short prologue that the nearest-pattern
search can otherwise select.

## Runtime acceptance

On 2026-09-27, the tester confirmed that the standalone PR build from commit
`c0ed7447ca335ef9695d9fc17ed7acf2278f6e4f` passed all requested in-game checks
on Steam client data revision 1076226 with Shroudtopia 0.1.1:

- Real terrain appeared correctly at the top-right.
- Walking updated the minimap location.
- Turning the camera while stationary updated the direction arrow.
- Tracking continued after leaving and rejoining the world and after a full
  game restart.

Tested DLL SHA256:
`4933E4DCA9C616DBDF6964BA5E5CB0E018F8459581150F700042B746AB52B0DD`.

The build contains this focused patch on upstream main, without PR #4's other
changes. Release x64 compilation and all 17 production-reader regression checks
also passed. Runtime acceptance is based on the tester's report; older client
layouts and extended sessions remain unverified.

## Vulkan command completion regression tests

```powershell
MSBuild tests\renderer_test.vcxproj /p:Configuration=Release /p:Platform=x64
.\build\tests\renderer_test.exe
```

The renderer test includes the production submission code with simulated Vulkan
callbacks. It checks delayed GPU completion across two swapchain images, unchanged
presentation waits when a draw is skipped, 100,000 submit/skip cycles, recording
and synchronization failures, and fence cleanup after waiting for idle. It does
not use a real GPU or establish long-session game stability.

The renderer now checks a per-image submission fence before resetting its command
buffer and skips the overlay frame while that fence is unsignaled. The fence is
reset only after recording succeeds and is passed to the overlay's queue submit.
An API error stops overlay submissions until the renderer is rebuilt. Presentation
continues using the original wait semaphores when no overlay was submitted.

This protects the command-buffer lifetime required by
[vkResetCommandBuffer](https://docs.vulkan.org/refpages/latest/refpages/source/vkResetCommandBuffer.html).
The existing per-image presentation semaphores are retained; a submission fence
does not itself prove that presentation has consumed a semaphore, as described in
[the Vulkan semaphore reuse guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).

The earlier runtime acceptance above applies to camera tracking only. The
combined-build gameplay acceptance is recorded below; extended-session results
have not been separately recorded.

## Swapchain recreation and driver faults

The 2026-10-04 extension covers the recreated-swapchain issue identified in
[PR #4](https://github.com/elxokker/Enshrouded-minimap/pull/4). Renderer reuse now
requires matching image handles when available and the same tracked creation
generation, even when the swapchain handle and dimensions are reused. A missing
image list alone does not trigger continuous rebuilding.

Access violations while recording or submitting stop overlay submissions until
the renderer is rebuilt, preserving the game's original presentation waits.
The code does not retry a faulting driver every frame. The production code,
precompiled header and tests use `/EHa` so C++ scopes, including mutex guards, unwind before
the narrow access-violation handler runs. Other exceptions are not swallowed.

Release/x64 validation with MSVC v143 passed all 27 renderer and 17 tracking
checks. These include handle reuse, creation generations, delayed completion,
access-violation containment and scope unwinding.

On 2026-10-04, the user confirmed that the combined PR #5/#6 build from
`0ab0ffd2a5a28a044b8cbcc5ca76c461979e429f` was working and requested that both PRs
be marked ready for review. The confirmation followed deployment of the new
DLL and selection of `icon_style: "original"`.

Tested DLL SHA256:
`A20012F47AE3C3D082C9760186BC78578DC40820503A21C6DD95B6A9C5C0A8CA`.

This is user-reported gameplay acceptance. Individual scene-change,
resolution-change, display-sleep and extended-session results were not
separately recorded. Synthetic renderer checks do not prove live fault recovery
or an FPS improvement, and other executable variants remain unverified.
