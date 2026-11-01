# GTA San Andreas Native

Objective: independent macOS 26+ ARM64 C++20 engine, Metal rendering, user-provided
classic PC assets. Preserve useful gta-reversed behavior. No Wine, Rosetta, hooks,
DLL injection or gta_sa.exe runtime. Do not begin open.mp work.

## Current implementation

M1 bootstrap acceptance verified on Apple M1. M2 has bounded IMG entry reads;
M3 renders textured DFF/TXD models with Metal. M4 has a first IDE/IPL world catalog,
binary IPL parsing and an 86-object Grove Street preview with continuous camera
movement and background region reload.
Gameplay is not
integrated. Residual M0 investigations
remain documented; do not stop useful isolated work merely because those are open.
Read docs/MILESTONES.md, ARCHITECTURE.md, GHIDRA_ANALYSIS.md, KNOWN_ISSUES.md.

## Build and verification

From repository root:
- `cmake -S native -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build/native`
- `ctest --test-dir build/native --output-on-failure` (GUI/Metal access needed)
- `open build/native/gta_sa_native.app`
- `build/native/gta_sa_assets "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.dff`
- `build/native/gta_sa_assets "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.dff --inspect-rw`
- `build/native/gta_sa_rw_probe "$GTA_SA_ASSET_DIR/models/gta3.img" infernus.dff infernus.txd`
- `build/native/gta_sa_world_probe "$GTA_SA_ASSET_DIR" 2489 -1668 13 80`
- `build/native/gta_sa_native.app/Contents/MacOS/gta_sa_native --assets "$GTA_SA_ASSET_DIR" --world-center 2489 -1668 13 80`

See native/README.md for LLDB and ASan/UBSan commands. Upstream root target remains
Windows/C++23; native target is separate and uses explicit source lists.
Asset installation audit: `python3 tools/audit_game_install.py "$GTA_SA_ASSET_DIR"`.
Keep installation paths in local shell/configuration, never tracked files.

## Ghidra

Use only `GTASanAndreasNative` under ignored local/ghidra. Never use unrelated
ps2samp (an accidental old import remains there; preserve its pre-existing data).
MCP static tools: list_instances, connect_instance, search_tools, check_tools,
list_tool_groups, load_tool_group, unload_tool_group, import_file. Dynamic tool
registration is broken in this client; actual documented localhost HTTP endpoints
work. Discover exact parameters from installed plugin source/schema, never guess.
Confirm correct project/program/hash before calls. Full reanalysis has already
produced 21,037 functions; do not repeat it without cause. The final API response
reported a persistence lock error; explicit subsequent save succeeded. Targeted
startup and directory functions are documented in GHIDRA_ANALYSIS.md and
REVERSE_ENGINEERING_PROGRESS.md. Preserve evidence, never fabricate signatures.

## Boundaries and conventions

Isolate Cocoa/Metal in Objective-C++; use portable C++ for engine/assets. Replace
fixed-address globals with owned state. Decode fixed-width disk structures explicitly;
do not force x86 runtime layouts onto ARM64 or disable assertions wholesale.
Follow upstream four-space/LF style, but its StaticRef rule does not apply to native
code. Tests must validate observable behavior and malformed resource boundaries.
Never satisfy tests with fake game implementations or silently disabled systems.
Do not commit proprietary binaries/assets, raw decompilation, local research clones
or Ghidra databases. Keep local/ ignored. This is a private, local project; license
investigation and redistribution preparation are outside the active work plan.
Update docs when implementation changes, report only executed verification.
