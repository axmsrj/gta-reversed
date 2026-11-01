# Ghidra analysis

## Environment and executable

Dedicated project: `GTASanAndreasNative`, under ignored `local/ghidra/`.
Ghidra 12.1.3, x86:LE:32:default, Windows compiler spec; GUI CodeBrowser launched
for `/gta_sa.exe` in this project. The earlier import into unrelated `ps2samp` was
an error; do not use that project. Its imported copy has not been removed.

Read-only executable identity:
- Size: 14,383,616 bytes; PE32/x86; little endian; image base 0x400000.
- SHA-256: `f01a00ce950fa40ca1ed59df0e789848c6edcf6405456274965885d0929343ac`.
- MD5: `2b5066bd4097ac2944ce6a9cf8fe5677`.
- Entry RVA 0x424570; VA 0x824570. Ghidra metadata agrees on image base/architecture.
- Exact release/patch identity remains unknown. Does not meet upstream Compact
  prerequisite of 5,189,632 bytes; individual functions must be checked independently.

## Tool connection and analysis quality

MCP list_instances/connect_instance verified connectivity. Static import_file works.
Dynamic tools remain absent from this client's tool catalog even though the server
reports registration. Verified local plugin source exposes HTTP endpoints; using
those on localhost restored program metadata, analysis, decompilation, disassembly,
xrefs, call graphs and saving without requiring user reconnection.

Full reanalysis grew the database from 5 to 21,037 functions. Final status reports
`analyzing=false`, `analyzed=true`; GUI log records Auto Analysis task completed.
The reanalyze HTTP result was nevertheless `success=false`: log gives
`Unable to lock due to active transaction` during persistence. A subsequent
save_program succeeded. Do not interpret analyzed=true as complete code coverage;
decompiler warnings occur in some regions. Findings below were inspected directly.
Local raw output lives only under ignored local/ghidra/findings.

## Verified startup observations

- PE entry 0x824570 calls CRT/helper functions and then 0x748710. Ghidra decompiler
  has stack tracking warnings here; no exact entry C signature is asserted.
- Disassembly at 0x748710 confirms Win32 system settings, process check, event
  dispatch through 0x619B60 with event 0x18 before window initialization, later
  input setup and event 0x15 for RenderWare initialization. Corresponding source:
  `source/app/platform/win/WinMain.cpp::NOTSA_WinMain`.
- 0x619B60 dispatches first to 0x53EC10 and handles unprocessed events in its
  fallback switch. Source correspondence: RenderWare skeleton event dispatch.
- At 0x53EC1F there is a jump to 0x4018CE before the switch dispatch at 0x53EC24.
  This detour is present in the supplied binary; its provenance is unknown.
  Do not blindly label this executable pristine or reuse every upstream address.
- 0x53BB50 calls 0x72F3B0, 0x56D180, 0x5386F0, 0x406B70(5), 0x541D90 in that
  order, then returns AL=1. Source comparison identifies memory initialization,
  localisation, filesystem, five CdStream channels and pad initialization.
  Existing source uses void return; binary returns success in AL. Caller use and
  exact ABI remain to be checked before changing the upstream signature.
- 0x5BD600 initializes pre-camera services, creates camera/frame/rasters, world,
  lighting/UI and working matrices; stores globals 0xC1703C (camera), 0xC17038
  (world), 0xB72920/24 (matrices), 0xBD0114 (previous restore callback).
  Camera raster fields are accessed at +0x60 and +0x64. These are original x86
  offsets, not ARM64 runtime layout requirements. Source `CGame::InitialiseRenderWare`.
- WinMain decompilation shows PeekMessage/TranslateMessage/DispatchMessage loop,
  state dispatch via global 0xC8D4C0, and cleanup including 0x53C900, event 0x16,
  DestroyWindow and event 0x19. This matches broad source responsibilities; transitive
  game initialization and shutdown behavior still needs incremental investigation.

## Verified directory behavior and layout

0x532350 opens a file, reads 4 header bytes, a 32-bit count, then 32-byte records.
Writes entries into owned directory storage while count < capacity. At 0x532450,
case-insensitive name search is linear, returning the first match or null. Names
start at record +8; 0x5324A0 returns record +0 position and uint16 at +4 size.
The directory object uses pointer +0, capacity +4 and count +8 on x86.
Compare `source/game_sa/Directory.{h,cpp}` and `Streaming.cpp::LoadCdDirectory`.
Native adaptation validates signature/count/bounds and decodes fixed-width fields
explicitly rather than copying the 32-bit runtime object layout.

## Targeted collision inspection

On the supplied binary, a read-only Ghidra 12.1.3 headless run opened the
dedicated `GTASanAndreasNative` project and confirmed MD5
`2b5066bd4097ac2944ce6a9cf8fe5677`, x86:LE:32 and image base 0x400000.
No full reanalysis or save was performed. The previous database had no functions
defined at the four selected collision addresses, so the script temporarily
disassembled and decompiled them; read-only changes were discarded.

- 0x412130 decompiled with sphere center/radius versus box min/max tests.
  A fresh read-only run on 2026-10-31 confirmed that after broad bounds checks,
  the routine computes a closest-point distance and compares it with the sphere
  radius (and a minimum-distance parameter). This supports using true distance
  for rounded box corners in the native swept-body query; the sweep itself is
  a native adaptation, not a claimed signature match.
- 0x416BA0 decompiled with compressed triangle-plane components and a sphere
  radius comparison. The parameter types are not established.
- 0x413070 decompiled as a thunk to 0x412C70 with line-versus-box bounds tests.
- 0x4185C0, the upstream `ProcessColModels` address, decompiled as a thunk to
  0x401C9D in this supplied executable. This is another detour/patch indication;
  do not assume the source function maps directly at this address.

Raw, untracked output: `local/ghidra/findings/collision_headless.txt`.
The fresh read-only output is `local/ghidra/findings/collision_headless_latest.txt`.

## Targeted animation inspection

A read-only headless run in `GTASanAndreasNative` on 2026-11-01 again confirmed
the same executable MD5 and x86 image base. The routine at 0x4CF2F0 finds the
largest final sequence timestamp, then subtracts adjacent frame timestamps in
reverse order. This confirms that the ANP3 file carries absolute frame times;
the game converts them to deltas after loading. The native sampler reads the
file directly, so its absolute-time interpolation is appropriate. 0x4D0240,
0x4D34F0 and 0x4D2E10 also decompiled; the latter checks an animation flag and
ped-position pointer before calling another routine. Their decompilation does
not establish the exact native bone-matrix composition or prove gait parity.
Raw ignored output: `local/ghidra/findings/animation_headless.txt`.

A second read-only run confirmed 0x735360, 0x4D2B90 and 0x7C5B80 decompile in
the same executable. At 0x735360 the root bone position is zero and later bone
positions are reconstructed from skin matrices while following HAnim parent
flags. At 0x7C5B80 the quaternion and translation are converted into a local
animation matrix. The native experiment that replaced every DFF bone matrix
with this animation matrix placed CJ horizontally; the clothing DFF's root
orientation must be retained in the current native coordinate setup. The
current adaptation uses the fixed root orientation and original-style child
bone positions, which match DFF local positions within 0.02 for all five
clothing pieces in the real-asset startup check. This does not prove exact
RenderWare skinning parity. Raw ignored output:
`local/ghidra/findings/animation_skin_headless.txt`.

## Remaining investigations

Exact executable release/patch provenance; complete RW engine open/start and
plugin registration order; critical object ownership/vtables; allocator behavior;
streaming concurrency and shutdown. Startup and directory findings establish a
basis for isolated bootstrap/asset work, not standalone gameplay readiness.
