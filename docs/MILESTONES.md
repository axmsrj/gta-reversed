# Milestone status

| Milestone | Status | Verified result / remaining work |
|---|---|---|
| M0 | Initial audit performed; open findings remain | Source, binary startup, directory behavior and librw code inspected; exact release provenance and deeper dependencies remain open |
| M1 | Acceptance criteria met on current host | ARM64 app, macOS 26 deployment target, Cocoa window, Metal on Apple M1, three frames and clean exit 0; no game exe dependency |
| M2 | Started | Independent IMG directory and bounded entry reads; portable 50 Hz game timer with pause/time scale and bounded frame step; memory/config/engine initialization not migrated |
| M3 | Started | Infernus geometry, three textures, four wheel instances and basic lighting drawn in Metal (3,822 triangles); full vehicle rules and world still required |
| M4 | Started | Text IDE/IPL and binary IMG IPL catalog; 95-object Grove Street preview at radius 80 with nine LOD substitutes, bounds selection, perspective camera and continuous movement, background region reload with cancellation, bounded model cache, 16/32-bit and DXT decoding; GTA distance rules and fine-grained streaming still required |
| M5 | Started | Bounded COL2/COL3 ground and box/sphere/triangle queries against real IPL placements; CJ's five original body parts render in Metal and blend `IDLE_stance` with `WALK_player`; simple axis sliding exists, while full pedestrian physics, other animations, vehicles and audio remain open |
| M6 | Not started | Single-player gameplay not implemented |

The isolated M1/bootstrap and verified directory extraction proceeded while residual
M0 findings remain open. No broad changes to upstream engine code were made.

## Reproducible validation

Commands: native/README.md. Apple Clang 21, CMake 4.4.3, Ninja 1.13.2.
- Clean build: arm64 Mach-O; LC_BUILD_VERSION minos 26.0, SDK 27.0.
- Metal smoke: Apple M1, 3 frames, exit 0; invalid argument exits 2.
- IMG test: valid lookup/sectors plus bad signature, excessive count, out-of-file
  offsets, directory overlap, unterminated name, truncated payload/header rejection.
- ASan/UBSan IMG suite and real gta3.img directory read: passed without reports.
- LLDB IMG suite: process exited 0.
- Real directories validated in place: gta3.img 16,316 entries; player.img 542;
  gta_int.img 2,484. infernus.dff offset 643,975,168 bytes, padded size 215,040 bytes.
- Real infernus.dff root is a clump (0x10) with 15 atomics. infernus.txd root is
  a texture dictionary (0x16) with 3 textures and device ID 2 (D3D9). Both were
  read from gta3.img in place and checked for nested chunk bounds.
- Entry-read limit, post-index file truncation and malformed chunk-length tests
  passed in normal and ASan/UBSan builds.
- Pinned librw built for ARM64 with its NULL backend. The native probe parsed the
  infernus clump (15 atomics) and texture dictionary (3 textures) successfully.
- The native app loaded real Infernus geometry and three TXD textures and completed
  three Metal frames with four wheel instances, provisional material tints, lighting,
  transparency and depth testing on Apple M1 (exit 0).
- World catalog decoded 14,204 IDE model types and 45,837 IPL instances, including
  binary `bnry` entries from `gta3.img`. Grove Street near (2489,-1668,13) at
  radius 80 selected 54 static pieces plus 32 props after examining nearby model
  geometry bounds, rendering 16,803 triangles and 148 textures across 3 frames on
  Apple M1 (exit 0). The selection margin and caps remain provisional.
- Camera orbit, elevation, pan, zoom and reset were verified in the app window.
- Holding E for 1.2 seconds moved the focus from (2489,-1668) to (2601,-1668);
  movement stopped on release. Holding C then moved it to (2601,-1503), with a
  new region becoming visible after the background load finished.
- Region rebuild now runs in a background task; the window continued drawing while
  preparing another region and exited cleanly after a close during a pending load.
- DXT1–DXT5 decoder tests cover RGB, transparency, alpha blocks and truncated
  input. Three consecutive real-asset Grove Street Metal runs passed after DXT
  integration (77 referenced textures, 3 frames, exit 0 each).
- Native 32-bit D3D textures (C888/C8888) and 16-bit C1555 are decoded with
  stride/level-size bounds checks. `sm_veg_tree4` rendered its leaves and bark
  correctly (132 triangles, 4 textures, 3 frames, exit 0); the region at
  (2601,-1503) also passed with 51 objects, 12,702 triangles and 127 textures.
- Camera projection moved from CPU vertex-buffer rewrites to a Metal vertex shader
  using a small per-frame uniform. Infernus, tree and 86-object Grove Street scenes
  passed 3-frame smokes; Infernus orbit and continuous Grove Street motion were
  inspected in the app window after the change.
- Grove Street perspective camera with aspect correction passed a 3-frame Metal
  smoke and was inspected while orbiting and moving across the region. The
  standalone model viewer retains its orthographic framing.
- The perspective shader now clips to a 0.1-unit near plane and a far plane
  covering the loaded geometry. The depth texture is retained across frames and
  replaced on drawable resize. A real-asset Grove Street 3-frame Metal smoke
  passed after both changes (86 objects, 16,803 triangles, exit 0).
- Transparent material groups are now sorted far to near for each camera frame
  using their world-space bounds centers; opaque groups retain depth writing.
  Grove Street completed a real-asset 3-frame Metal smoke after this change
  (86 objects, 16,803 triangles, exit 0). Per-triangle transparency remains open.
- Each draw group has a world-space box and is skipped if fully outside a camera
  clip plane. Camera visibility tests cover perspective sides, near/far planes,
  near-plane crossing and orthographic cases; all five non-GUI tests pass. Grove
  Street completed three Metal frames after culling, and a no-asset three-frame
  smoke passed after extracting the camera logic to a portable test header.
- The DFF adapter now omits empty material groups. The real Grove Street smoke
  retained 16,803 triangles and 148 textures while reducing the scene from 2,740
  material slots to 276 nonempty draw groups (3 frames, exit 0). IDE `objs` and
  `tobj` entries now retain draw distance and flags; the real catalog still parses
  14,204 types and 45,837 instances, and all five non-GUI tests pass.
- IPL source identity is retained so streaming IPL LOD indices can resolve against
  their matching base text IPL. All 6,067 LOD references in the real catalog
  resolved; linked LOD targets are now excluded from the high-detail preview pass
  even when their model names do not begin with `LOD`. A separate fallback pass
  now loads LOD targets lacking a selected high-detail child. Grove Street added
  nine fallback objects and completed three Metal frames (95 objects, 17,317
  triangles, 157 textures, exit 0). GTA distance transitions and streaming policy
  remain open. The resulting Grove Street scene was inspected in the native window;
  roads, houses and drainage structures appeared without an obvious duplicate LOD
  overlay in that view.
- A completed background region is now discarded if the focus has moved at least
  25 units since its request. In an interactive run, holding E moved focus from
  (2489,-1668) to (2716,-1668); the finished (2515,-1668) region was logged as
  stale and skipped, and the later (2696,-1668) region became visible in the
  native window. This avoids uploading outdated geometry during fast movement.
- The background loader now checks the moving focus between object loads and
  cancels obsolete regions early. Holding E for 2.5 seconds moved focus from
  (2489,-1668) to (2718,-1668); five intermediate loads were logged as cancelled,
  then the final region loaded 45 objects and appeared in the native window.
- Added an owned portable game timer using the original 50 Hz timestep unit,
  three-step simulation cap and 300 ms accumulated-time cap. Its tests cover
  ordinary, slow, scaled and paused frames plus invalid input; the native loop
  now obtains frame deltas through it. Gameplay still does not consume its
  simulation step.
- Collision groundwork: bounded COL2/COL3 parser and ground-height query use
  compressed vertices and IPL transforms. Audit of all 216 COL entries in
  `gta3.img` parsed 8,255 model records and 1,058,484 triangles with zero errors.
  A real Grove Street query at (2489,-1668) found `Lae2_roads89` at Z=12.344;
  near CJ's house, a query capped at Z=16 found Z=13.766. Parser and geometry
  tests cover bounds, indices, transforms and layered surfaces. No player
  collision or physics behavior is claimed yet.
- The native Metal process now owns a collision catalog loaded once from
  `gta3.img` (8,255 models, 1,058,484 triangles). Its startup ground query found
  `Lae2_roads89` at Z=12.344 in Grove Street, and the real-asset scene completed
  three Metal frames with exit 0. Region reloads also query ground height; entity
  collision responses are still absent.
- `player.img` contains the default CJ body as separate head, torso, legs, feet
  and hands DFF/TXD pairs. A player-preview mode now assembles all five with
  clothing textures; the special clothing path leaves skin-space vertex positions
  untransformed, fixing an initially horizontal body. The upright result was
  inspected in the window and completed three Metal frames (2,366 triangles,
  five textures, exit 0). The standalone preview remains a static pose.
- `--play` now places CJ on the collision ground in Grove Street and moves the
  camera focus with WASD relative to camera yaw. Walking rejects missing ground,
  steps over 0.6 units and drops over 1.5 units. An interactive W press moved
  the window's world position from (2489,-1668) to (2500,-1662). The character
  turns toward successful travel, and the scene updates only its player vertices
  and draw bounds. The real-asset Metal smoke completed three frames with 19,683
  scene triangles, 162 textures and exit 0; all nine non-GUI tests passed.
- A portable bounded ANP3 reader parses `anim/ped.ifp` without x86 layouts.
  The real file yielded 294 clips, 7,607 bone sequences and 112,150 decoded
  keyframes. Frame ranges, names, counts, allocation sizes and compressed frame
  types are checked; malformed-file tests cover these boundaries.
- Registered librw's HAnim and Skin plugins before opening the engine. The
  original default CJ head, torso, legs, feet and hands DFFs each expose the
  same 37-node hierarchy and skin weights (6, 16, 7, 5 and 8 used bones).
  The `--play` real-asset Metal smoke still completed three frames, exit 0.
- The portable clip sampler now reads the IFP timestamps directly, interpolates
  normalized shortest-path quaternions and translations, and loops at clip
  duration. Every clip in the real `ped.ifp` decoded and sampled at midpoint;
  `WALK_player` lasts 1.2 seconds and supplies 32 bone poses.
- `--play` now retains each clothing part's vertex weights and 37-bone bind
  matrices, verifies the undeformed matrices reduce to identity, and applies
  `WALK_player` pose changes to the player vertices each walking frame. Using
  rotations relative to the first clip frame preserves the upright DFF bind
  orientation; direct rotations initially laid CJ sideways and were corrected.
  In the live Metal window, CJ stayed upright with a visible walking leg pose
  while moving along Grove Street. An automated diagnostic walk moved the world
  focus from (2491,-1667) to (2507,-1656). After draw-bound updates, the real
  asset smoke completed three frames and exited 0; all 10 non-GUI tests passed.
- A targeted read-only Ghidra run in the dedicated project inspected original
  sphere/box, sphere/triangle, line/box and general collision addresses. The
  supplied binary's 0x4185C0 is a thunk to 0x401C9D, so source correspondence
  remains qualified; details are in GHIDRA_ANALYSIS.md.
- COL2/COL3 parsing now retains bounded box and sphere primitives. All 216
  real COL entries parsed (8,255 models, 1,058,484 triangles, 1,912 spheres,
  30,993 boxes, zero failures). The controller sweeps two body-height spheres
  against nearby transformed box/sphere volumes before accepting a ground step.
  Unit tests cover valid and malformed primitive data, rotated instances,
  pass-through and blocking. The real-asset Metal smoke exited 0 after three
  frames. The first interactive auto-walk appeared to cross a fence and stopped
  at a house volume; the path was investigated further below.
- A short body-sphere sweep now also checks COL triangles. Tests cover a vertical
  wall, open path and floor that should not block at torso height. A subsequent
  real-asset auto-walk remained stable and reached the same house position.
  The new volume probe found four `DYN_F_R_WOOD_1b` fence instances nearby.
  The observed diagonal path at (2509.8,-1654.2) misses their boxes; a direct
  perpendicular segment from (2514,-1657) to (2514,-1651) at ground Z=12.846
  intersects the fence at (2514.73,-1654.19). The earlier screenshot therefore
  does not establish fence penetration. Full collision response and exact GTA
  pedestrian capsule behavior remain open. The post-change real-asset Metal
  smoke completed three frames, exit 0; all 11 non-GUI tests passed.
- A fresh read-only Ghidra inspection in the dedicated project confirmed that
  the original sphere/box routine compares closest-point distance with radius.
  The native swept-box check now uses its expanded bounds only as a rejection
  filter and measures true segment-to-box distance at rounded corners. A corner
  false-positive regression and the existing blocking checks pass; all 11
  non-GUI tests passed after rebuilding.
- The world catalog now builds a 32 m XY cell index after loading. Nearby
  queries still apply the original 3D radius, interior and model filters and
  return instances in catalog order. The real installation loads 45,837
  placements and yields 155 at Grove Street within 80 m. Indexed query,
  cross-cell ordering and index invalidation after an IPL append pass tests;
  all 11 non-GUI tests passed after rebuilding. The real-asset Metal run drew
  three frames and exited 0 with 19,683 loaded triangles and 162 textures.
- The ground-constrained controller now tries each axis separately after a
  blocked diagonal step. It keeps the available component, checks ground and
  swept collision again for each part, and faces the actual travel direction.
  Tests cover sliding along a wall and no movement when both paths are blocked;
  all 11 non-GUI tests passed. The real-asset Metal smoke drew three frames and
  exited 0. This is a simple axis response, not original GTA contact resolution.
- `ped.ifp` also supplies `IDLE_stance` (32 posed bones, 1.5 s). The play mode
  now samples that clip continuously while stopped and blends it with
  `WALK_player` over 0.18 s when movement starts or ends. The live Metal window
  showed CJ upright at rest, moving after a W press, and returning to rest.
  The pose blend handles opposite-sign equivalent quaternions; all 11 non-GUI
  tests passed. The live inspection process was then stopped manually.
- User inspection found that the first idle and walking poses were visibly
  wrong and that nearby houses used LOD assets. The root of `WALK_player`
  advances 1.867 units over 1.2 s; adding this to controller movement caused
  drift. Native skinning now omits root translation, grounds the lowest skinned
  vertex, and uses the first `IDLE_stance` pose as the shared orientation
  reference. Controller speed is now derived from the loaded clip's root travel
  (1.55599 units/s for the inspected `ped.ifp`) and rejected when invalid. The
  latest window inspection showed CJ with arms and legs resting naturally;
  exact original gait and foot contact are still unverified. The real-asset
  auto-walk Metal smoke drew 30 frames and exited 0 after this change; all 11
  non-GUI tests passed.
- Walking movement now integrates the root's measured forward travel between
  successive `WALK_player` samples, including a cycle wrap, rather than using
  only its average speed. Blocked movement leaves the clip clock in place.
  Synthetic intervals, wraparound, and controller distance injection pass the
  11 non-GUI tests; the real-asset auto-walk Metal smoke drew 30 frames and
  exited 0. Visual gait parity and foot contact remain open.
- The world preview now reserves a pass for near detailed children of LOD
  placements before small props. At Grove Street the real-asset log changed
  from nine LOD objects to one distant LOD road; `compfukhouse3` and
  `ganghous01/02_LAx` detail DFFs load in place of nearby LOD houses. Their IDE
  texture dictionaries are `comedhos1_la` and `ganghouse1_lax`, not the LOD
  dictionaries. The Metal smoke rendered 21,984 triangles and 173 textures in
  three frames, exit 0. All 11 non-GUI tests passed after the animation changes.
- A continuous auto-walk capture exposed a still-stiff leg/arm pose after the
  root-motion and speed fixes. Reversing the relative quaternion multiplication
  order in the native skin transform improved the observed leg and arm stance;
  this is a visual comparison, not verified GTA animation parity. Ghidra
  read-only inspection of 0x4CF2F0 confirmed on-disk absolute ANP3 timestamps
  are converted to deltas by the original runtime, so the native file sampler's
  timing was retained. All 11 non-GUI tests pass, and the final real-asset
  Metal smoke drew three frames and exited 0.
- Targeted source and read-only Ghidra inspection of 0x735360/0x7C5B80 guided
  a closer skinned-ped transform: child bone positions are rebuilt from the
  inverse skin matrices and child rotations come from the sampled clip. Direct
  application at the root made CJ horizontal, so the native path retains the
  DFF's fixed root orientation and applies relative root turning. Bone positions
  matched the DFF local hierarchy within 0.02 for all five real clothing parts;
  the latest Metal smoke drew three frames, exit 0. The walking pose still
  needs original-game comparison and foot-contact validation.
- Direct probes of the installed assets show the first `IDLE_stance` root
  quaternion is approximately (0.008,0,0.708,0.706), while the torso DFF's
  root basis cycles the coordinate axes. This explains why applying clip
  orientation to the root without a coordinate correction lays the model down.
  The current hybrid keeps this basis while applying the source-supported
  child-bone positions. All 11 non-GUI tests passed after the change.
- A 30-frame real-asset auto-walk profile measured about 9 ms at its sampled
  frame (3.0 ms movement/collision, 1.4 ms skinning, 4.0 ms remaining work), so
  frame cost alone does not explain the stiff pose in that segment. Source
  `CAnimBlendNode` uses quaternion slerp; the native sampler and pose blender
  now use shortest-path slerp instead of normalized linear interpolation.
  A quarter-frame rotation check and all 11 non-GUI tests passed, all 294 real
  IFP clips sampled successfully, and a 30-frame Metal auto-walk exited 0.
  Visual verification of the new idle pose was unavailable because macOS
  locked the session during inspection; exact original gait remains open.
- Only macOS/system frameworks and libraries are linked by the native application.
- GUI smoke initially aborted under restricted sandbox; passed with WindowServer access.

The application is a development bootstrap; it does not run GTA gameplay.
