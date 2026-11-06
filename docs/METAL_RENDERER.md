# Metal renderer

Implemented M1 in `native/Main.mm`: Cocoa application/window, CAMetalLayer, Metal
device and command queue, drawable-sized render pass, clear/store, presentation,
command completion/error checks, per-frame autorelease pool and controlled cleanup.
The optional `--assets` path reads `infernus.dff` and `infernus.txd` from `gta3.img`,
decodes 15 atomics and three 16-bit textures with librw, applies frame transforms,
instances the wheel mesh at four dummy positions, and draws 3,822 triangles with
Metal shaders, depth testing and basic lighting. Transparent material groups render
after opaque geometry, ordered far to near by their bounds center each frame. This
is a group-level approximation; intersecting triangles within one group still need
finer sorting or order-independent transparency. The adapter handles DXT1–DXT5 plus 1555/565/4444 and 888/8888 textures and generates
Metal mipmaps. Paint, glass and untextured white surfaces use provisional tints.
A Grove Street preview at radius 80 uses IDE/IPL placements, props, alpha textures,
geometry-bounds selection and a keyboard camera; its high-detail passes select 86 objects.
The Grove Street preview has a LOD fallback pass: when a linked low-detail
target has no selected detailed child, it can fill the gap. The current radius
80 scene loads 165 objects after geometry bounds selection across the full
160-unit candidate margin. Transitions still need GTA distance rules and
finer streaming.
Holding Q/E/Z/C moves the focus continuously and triggers background region rebuilds.
When a completed region is already at least 25 units behind the current focus,
the app discards that scene and immediately requests the current region instead
of uploading stale geometry to Metal. The background loader also checks the
latest focus between objects and cancels obsolete work early.
The vertex buffer now stores model/world coordinates once per scene. The Metal
vertex shader applies orbit, elevation, pan, zoom and depth projection from a
17-float camera uniform each frame; the CPU computes that uniform from bounds
captured on scene reload. The world preview now uses a perspective camera with
aspect correction, a 0.1-unit near plane and a far plane covering loaded geometry;
standalone model viewing remains orthographic. The depth target is reused until
the drawable size changes. Each draw group now has a world-space box; groups fully
outside any camera clip plane are skipped conservatively. GTA camera follow behavior
and world-object-level visibility selection remain open.
`--play` now deforms CJ's original clothing meshes with the `WALK_player` clip
and updates player draw bounds while he moves. Other animation states, full
vehicle material rules, fine-grained world streaming and gameplay remain open.

Cocoa was selected for bootstrap because it provides direct layer/window lifecycle
without a new external runtime. C++ game/asset code stays outside Objective-C++.
Initial frame scheduling deliberately waits for completion to bound in-flight
resources; optimize after functional rendering and resource lifetime tests exist.

Validated on Apple M1: three completed Metal frames with real Infernus geometry/textures,
normal exit 0. Test requires
WindowServer access; the restricted sandbox aborts before initializing the GUI.
Closing the window ends the interactive loop. Smoke mode fails on premature close,
GPU error, unavailable drawable or incomplete frame count.

Commands and test details: `native/README.md`. The model smoke test verifies asset
decoding and GPU command completion; it does not establish gameplay support.
