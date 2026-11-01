# RenderWare code audit

Local `source/game_sa/RenderWare/rw/rwcore.cpp` delegates many operations to absolute
executable addresses. `CGame::InitialiseRenderWare` directly manages D3D resources
and restore callbacks. Binary camera/world allocation and callback storage were
inspected; see GHIDRA_ANALYSIS.md. Headers alone are not an independent RW engine.

## librw revision inspected

Public `aap/librw`, commit `18532c2e13efbc1aa43f0b00b4459831f9414de0`,
held in ignored `local/librw`. The native parsing probe and Metal application link
this revision; CMake fetches that commit when the local checkout is absent.
License and redistribution work is outside this private project.

| Capability | Concrete source inspected | Assessment |
|---|---|---|
| DFF geometry/materials | src/geometry.cpp::Geometry::streamRead | Real Infernus and 26 static Grove Street pieces parsed and rendered; broader model coverage remains |
| TXD/native textures | src/texture.cpp::Texture::streamReadNative, src/d3d/d3d9.cpp::readNativeTexture | Dispatches platform streams; D3D9 fields/mips/palette/compressed flags present; not a Metal uploader |
| Skinning | src/skin.cpp, src/rwplugins.h, src/d3d/d3d9skin.cpp, src/gl/gl3skin.cpp | Skin objects and backends present; SA split-skin compatibility unverified |
| HAnim/UV animation | src/hanim.cpp, src/uvanim.cpp, src/rwanim.h | Hierarchy and animation plugin foundations present |
| Material effects | src/matfx.cpp, backend matfx implementations | Foundation exists; full SA effects unproven |
| Camera/lights/world | src/camera.cpp, src/light.cpp, src/world.cpp | Object infrastructure present; full SA world behavior unverified |
| Alpha/depth/stencil/blend states | src/rwrender.h | Explicit state identifiers; semantics need Metal mapping and validation |
| Texture sampling | src/texture.cpp, src/d3d/d3d9.cpp | Filter/address state retained; exact Metal mapping pending |
| Native Metal | root CMakeLists.txt, src/CMakeLists.txt | No Metal selection/backend in inspected build; macOS choices NULL or GL3 |
| SA custom pipelines | source/app/app.cpp::PluginAttach (gta-reversed) | Building/car/visibility/animation-blend/breakable plugins remain integration requirements |

D3D9 texture reader asserts on cube maps and has a TODO for unsupported format
conversion. Compressed upload currently creates a backend texture. Do not assume
cross-platform parsing alone provides GPU-independent texture decoding. Stream
readers also require defensive boundaries before accepting arbitrary files.

Provisional architecture: reuse librw's object/stream/plugin foundation through an
adapter and implement a native Metal backend; re-evaluate after a real DFF/TXD
integration spike. This likely avoids rewriting generic RW objects, but requires
materially more than choosing a build option. Do not use GL3 or D3D translation as
the final renderer. The graphical app uses librw with its NULL backend for asset
parsing, then uploads decoded textures and transformed triangles to Metal. It is
not a full librw renderer.

The native probe compiled librw for ARM64 with its NULL backend and parsed the real
`infernus.dff` and `infernus.txd` in place: 15 atomics and three textures. This
confirms stream parsing for that pair; no Metal resource was created by the probe.
