# Direct source port direction

The target is the original single-player game behavior running as a native
macOS ARM64 executable with Metal and user-provided PC assets. The existing
`gta-reversed` game systems are the source of truth for gameplay, timing,
streaming, camera, animation, collision, scripts and audio. `native/` is a
platform bootstrap and a set of asset/renderer probes, not a second game
implementation to be expanded until it resembles GTA San Andreas.

## Current boundary

The root target is a Windows module by default. Its standalone option is not
a working game executable. `source/app/platform/win/WinMain.cpp` still uses
Win32 startup and messages; numerous source files use `StaticRef` or calls to
addresses in `gta_sa.exe`; rendering paths use D3D9. The decompiled percentage
therefore does not measure standalone portability or gameplay completeness.
These dependencies must be replaced by native state and implementations before
the original game loop can run independently on ARM64.

## Migration order

1. Build an explicit inventory of source systems reachable from the original
   startup and main loop. Classify each dependency as reusable C++, fixed
   address state/call, Win32 service, D3D9/RenderWare backend, or missing
   implementation. Verify uncertain binary behavior in `GTASanAndreasNative`.
2. Create a native target that compiles the actual reusable `source/` files,
   one coherent subsystem at a time. Keep its macOS platform and Metal code
   behind interfaces; move fixed-address globals into owned state without
   changing game rules.
3. Bring up the original initialization, timer and frame loop with original
   asset loading and scripts. Use the existing `native/` probes as comparison
   tools, retiring duplicated gameplay paths when their source counterparts
   run.
4. Connect the RenderWare-facing draw behavior to a Metal backend, preserving
   original materials, ordering, camera and visibility decisions. Validate
   against original asset data and recorded game behavior at each subsystem
   boundary.
5. Continue through input, player tasks, pedestrians, vehicles, audio,
   missions and saves until the original single-player behavior runs. Do not
   treat a Grove Street preview or a moving CJ as a completed game.

Every migrated subsystem needs an executable verification that exercises its
real behavior. Keep Ghidra evidence and source correspondence in
`GHIDRA_ANALYSIS.md`. Do not import the Windows executable as a runtime
dependency, and do not introduce Wine, Rosetta, injection or open.mp work.
