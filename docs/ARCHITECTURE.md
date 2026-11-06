# Native port architecture — preliminary M0 audit

The implementation direction is now a direct port of the reusable
`gta-reversed` game systems. The separate `native/` executable remains a
bootstrap and validation harness, not the intended gameplay implementation.
See [DIRECT_SOURCE_PORT.md](DIRECT_SOURCE_PORT.md) for the migration order.

Audited baseline: `937d7a5e748c9bcca56c52c3fb0a2ea99630bc4e`, 2026-10-25.
M0 has remaining open findings. The native bootstrap builds and runs; gameplay is not integrated.

`CMakeLists.txt` selects C++23; `source/CMakeLists.txt` recursively gathers
engine, application, hooks and platform code into one MODULE target by default.
`GTASA_STANDALONE` switches target type, but `source/app/platform/win/WinMain.cpp`
explicitly rejects normal standalone execution; only hook dumping is implemented.
SDL3 support still requests a Win32 HWND for RenderWare.

## Dependency inventory

| Root / function | Observed dependency | Classification |
|---|---|---|
| `source/dllmain.cpp::DllMain` | Windows process attachment | Platform specific |
| `source/InjectHooksMain.cpp` | Registers engine replacement hooks | Binary integration |
| `source/Base.h::StaticRef` | Dereferences original absolute address | Original binary state |
| `source/PluginBase.h::Call`, `CallMethod` | Calls absolute executable addresses | Delegated implementation |
| `source/app/platform/win/WinMain.cpp::WinMain` | Hook dump or error exit | Incomplete standalone entry |
| `source/app/platform/win/WinMain.cpp::NOTSA_WinMain` | Win32 events, HWND, game globals | Reconstructed but binary dependent |
| `source/game_sa/Game.cpp::InitialiseOnceBeforeRW` | Memory, localisation, files, CdStream, pad | Reconstructed orchestration; transitive audit required |
| `source/game_sa/Game.cpp::InitialiseRenderWare` | D3D resources, texture store, camera, world, UI | Binary and D3D dependent |
| `source/game_sa/RenderWare/rw/rwcore.cpp::RxHeapCreate` | Calls 0x809F90 | Delegated; address from source only |
| `source/game_sa/MemoryMgr.cpp::Init` | Conditional heap code; hook entries marked unreversed | Partial/configuration dependent |
| `source/game_sa/Shadows.cpp` | x86 inline assembly and executable call | Architecture specific |

No whole subsystem is certified independently runnable by this audit.
Hook metadata and comments alone are evidence of source intent, not binary verification.
Specific startup and directory functions have now also been compared in Ghidra;
see GHIDRA_ANALYSIS.md for those verified observations.

## Implemented native boundary

`native/CMakeLists.txt` is a separate C++20/Objective-C++ project with explicit
source lists. `gta_sa_native` links Cocoa, Metal and QuartzCore only; it uses no
StdInc, injected engine target or address-based globals. `gta_sa_asset_core` adapts
verified IMG directory behavior with owned state. `gta_sa_assets` diagnoses real
archives without launching the graphical application. Build/test: native/README.md.

## Extraction boundaries

Keep upstream source behavior as the reference. Bring reusable source files into
an explicit native C++20 / Objective-C++ target with Cocoa/Metal platform
services. Replace binary-backed globals and calls with owned state and native
implementations at the subsystem boundary. Do not link address-based globals
or calls into the native target.

Dependency graph (source-derived): native entry → platform/logging → renderer;
engine entry → memory/files/CdStream/input → RW engine/plugins/device →
texture store/camera/world → game initialization/streaming → idle/gameplay.
This is a dependency outline, not a validated original startup sequence.
