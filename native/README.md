# GTA San Andreas Native — development build

This is an independent Apple Silicon application with an initial RenderWare model
viewer and neighborhood preview. It reads original IMG/IDE/IPL assets and
renders them with Metal. Basic CJ ground-constrained movement is available;
full gameplay is not integrated. The existing injected
Windows target is unchanged. See ../docs/MILESTONES.md for verified status.

Requirements: Apple Silicon, macOS 26+, Xcode command line tools, CMake 4.2+, Ninja.

From the repository root:

```sh
cmake -S native -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native
ctest --test-dir build/native --output-on-failure
open build/native/gta_sa_native.app
```

The GUI smoke test needs a logged-in graphical session with Metal access. It presents
three frames and exits. In a restricted sandbox, run it with WindowServer access.
For direct diagnostics:

```sh
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --smoke-frames 3
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR"
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --smoke-frames 3
build/native/gta_sa_assets "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.dff
build/native/gta_sa_assets "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.dff --inspect-rw
build/native/gta_sa_assets "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.txd --inspect-rw
build/native/gta_sa_rw_probe "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.dff infernus.txd
build/native/gta_sa_world_probe "$GTA_SA_ASSET_DIR" 2489 -1668 13 80
build/native/gta_sa_collision_probe "$GTA_SA_ASSET_DIR/models/gta3.img" --all
build/native/gta_sa_ground_probe "$GTA_SA_ASSET_DIR" 2489 -1668 100
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --instance-id 17613
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --world-center 2489 -1668 13 80
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --player-preview
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --world-center 2489 -1668 13 80 --play
build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --world-center 2489 -1668 13 80 --play --auto-walk
build/native/gta_sa_ifp_probe "$GTA_SA_ASSET_DIR/anim/ped.ifp"
build/native/gta_sa_rw_probe "$GTA_SA_ASSET_DIR/models/player.img" head.dff head.txd
build/native/gta_sa_volume_probe "$GTA_SA_ASSET_DIR" DYN_F_R_WOOD_1b 2514 -1657 12.846 2514 -1651 12.846
```

Set `GTA_SA_ASSET_DIR` in your shell to your own classic PC installation directory.
The asset command reads IMG directory metadata and, with `--inspect-rw`, reads only
the selected entry and checks RenderWare chunk boundaries. It does not extract assets.
The probe uses pinned librw with its NULL backend to parse a real DFF/TXD pair.
It loads `local/librw` when present; CMake fetches the pinned commit otherwise.
The probe does not render the model.
The world probe combines text IDE/IPL files from `gta.dat` with binary `bnry` IPL
entries from `gta3.img`. `--world-center` selects nearby terrain, detailed
objects and props without fixed per-category caps, then uses LOD substitutes
where detail is unavailable. Geometry-bounds selection uses a 160-unit
candidate margin; Grove Street at radius 80 loads 165 objects
(31,364 triangles, 194 textures with CJ).
The preview uses a perspective camera; standalone model viewing remains orthographic.
The world process also loads collision meshes from `gta3.img` and samples real
ground height at the focus. In `--play`, it also constrains CJ's basic walking.
In the window, A/D or left/right orbit,
W/S or up/down change elevation, +/- or scroll zoom, I/J/K/L pan and R resets the
camera. In world preview, hold Q/E to move west/east and Z/C to move south/north
at 90 world units per second; diagonal speed is normalized. Releasing the key stops
movement. After 25 units, the app rebuilds nearby pieces in the background and
caches decoded models within a 256 MiB budget. The current scene keeps drawing
while it loads; obsolete loads cancel between objects. A portable 50 Hz game
timer supplies bounded frame steps, though gameplay is not scheduled from it yet.
Fine-grained streaming and playable world simulation remain open.
The application does not open or execute gta_sa.exe. Without `--assets`, it displays
the original blank Metal bootstrap frame. With `--assets`, it reads `models/gta3.img`
and draws the 15-part Infernus with four wheel instances, its three TXD textures,
basic lighting, depth testing and transparent materials. The adapter decodes 1555,
565, 4444, 888, 8888 and DXT1–DXT5 base-level textures and generates Metal mipmaps.
Vehicle paint and otherwise
white untextured materials use provisional tints; this is still a model viewer.
`--player-preview` reads the five default CJ clothing meshes and TXDs from
`models/player.img`; it shows a static textured pose without animation or input.
`--play` combines those parts with the Grove Street region. WASD moves relative
to camera yaw, and the camera follows CJ. `run_player` drives ordinary movement;
hold Option/ALT for `WALK_player`. CJ turns toward travel over time. The camera
starts behind CJ, the captured mouse rotates it, Escape releases the cursor,
and a click recaptures it. A/D turn CJ while the camera keeps its angle and
follows his position.
Idle keeps the original `IDLE_stance` bone animation and a stable grounding
offset; the port still needs original foot placement on varied ground.
`--auto-walk` drives the same controller along the road for visual diagnostics.
Ground and basic COL box/sphere/triangle checks constrain movement. The volume
probe can test a specific path against nearby instances of one model. Idle
full contact response, jumping, vehicles, audio and missions are not
implemented. Blocked diagonal walking can slide along an unblocked axis.
The original `IDLE_stance`, `WALK_player`, and `run_player` clips are blended
during movement transitions. The original player shoulder adjustment is applied
to the breast bones after pose evaluation.
The IFP probe validates and decodes the original ANP3 ped animation frames;
the walking clip now drives the player's skin in the renderer.

Memory safety and debugger checks:

```sh
cmake -S native -B build/native-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGTASA_NATIVE_SANITIZERS=ON
cmake --build build/native-asan --target img_directory_tests gta_sa_assets
ctest --test-dir build/native-asan -R img_directory --output-on-failure
xcrun lldb --batch -o run -o 'process status' -- build/native/img_directory_tests build/native/img-tests-lldb
file build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native
xcrun vtool -show-build build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native
```

Keep original game files and Ghidra databases local and untracked. This project is
private; redistribution and license preparation are outside the active work plan.
