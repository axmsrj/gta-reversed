# Reverse engineering progress

Addresses below were inspected in the supplied executable via Ghidra. Names are
source correspondences, not claims that original debug symbols exist.

| Function | Original Address | gta-reversed Location | Status | Notes |
|---|---|---|---|---|
| PE/CRT entry | 0x824570 | no portable entry previously | Inspected | Calls 0x748710; decompiler stack warnings |
| NOTSA_WinMain correspondence | 0x748710 | source/app/platform/win/WinMain.cpp | Disassembly/decompilation compared | Native bootstrap now separate; game loop not migrated |
| Event dispatcher | 0x619B60 | source/app/platform/platform.cpp | Decompiled | Calls app handler then fallback |
| AppEventHandler correspondence | 0x53EC10 | source/app/app.cpp | Inspected with caveat | Supplied binary detours via 0x4018CE |
| CGame::InitialiseOnceBeforeRW | 0x53BB50 | source/game_sa/Game.cpp | Disassembly/decompilation compared | Five initialization calls; AL success return differs from source void |
| CGame::InitialiseRenderWare | 0x5BD600 | source/game_sa/Game.cpp | Disassembly/decompilation compared | RW/D3D dependencies remain |
| CDirectory::ReadDirFile | 0x532350 | source/game_sa/Directory.cpp | Compared; adapted in native/assets | Fixed-width records and owned storage; hardened bounds |
| CDirectory::FindItem(name) | 0x532450 | source/game_sa/Directory.cpp | Compared; adapted in native/assets | First ASCII case-insensitive match |
| CDirectory::FindItem(name,pos,size) | 0x5324A0 | source/game_sa/Directory.cpp | Compared | Position +0, uint16 size +4 |
| ProcessSphereBox correspondence | 0x412130 | source/game_sa/Collision/Collision.cpp | Read-only Ghidra disassembly/decompilation | Sphere radius versus box extents visible; exact signature unverified |
| ProcessSphereTriangle correspondence | 0x416BA0 | source/game_sa/Collision/Collision.cpp | Read-only Ghidra disassembly/decompilation | Compressed plane and sphere radius comparisons visible |
| TestLineBox correspondence | 0x413070 | source/game_sa/Collision/Collision.cpp | Read-only Ghidra disassembly/decompilation | Thunk to 0x412C70; line/box bounds comparisons |
| ProcessColModels correspondence | 0x4185C0 | source/game_sa/Collision/Collision.cpp | Read-only Ghidra disassembly/decompilation with caveat | Thunk to 0x401C9D in supplied binary; patch provenance unknown |
| RxHeapCreate | Unverified (source 0x809F90) | source/game_sa/RenderWare/rw/rwcore.cpp | Source-only delegated function | Not independently implemented |

See GHIDRA_ANALYSIS.md for limitations. Native IMG adaptation has synthetic behavior,
malformed-input, real-archive and ASan/UBSan tests; this does not validate world streaming.
