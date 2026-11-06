# Known issues and constraints

- Executable is not the Compact size required upstream. Exact release/patch provenance
  remains unknown. A detour in the app event handler is verified in the binary.
- Ghidra dynamic tools are absent from the client catalog. Local documented HTTP
  endpoints work. Reanalysis completed useful work but reported a transaction-lock
  persistence error; subsequent explicit save succeeded. See GHIDRA_ANALYSIS.md.
- Native Metal code renders a textured vehicle and a small world preview.
  World preview has orbit/pan/zoom, continuous focus movement, geometry-bounds
  selection and coarse background region rebuilds, but lacks fine-grained streaming
  and full LOD/alpha behavior (transparent groups are sorted, but their triangles are not),
  so visible areas can contain gaps. It also lacks
  audio and gameplay. GUI tests need WindowServer access.
- The portable timer provides GTA-compatible timestep units, but no gameplay
  systems are scheduled from it yet.
- COL2/COL3 collision meshes and box/sphere primitives are loaded into the
  running app. Ground queries and basic box/sphere/triangle checks constrain
  CJ's walking. A diagonal route initially appeared to cross a fence, but its
  measured path misses the nearby COL boxes; a perpendicular query intersects
  the fence. Simple axis sliding exists; full contact response, falling, jumps
  and material responses are not implemented. Catalog loading is eager, not
  spatially streamed.
- CJ's original clothing pieces deform under `WALK_player` while moving and
  `IDLE_stance` while stopped. The initial stance frame anchors the root
  orientation, with a short spherical rotation blend at transitions. Child bone
  positions come from inverse skin matrices and their rotations from the clip.
  Forward walk/run root travel is extracted into the controller; local root X/Z
  motion and idle root translation remain in the skin pose. Moving vertices are
  still grounded from their lowest point each frame. This still does not
  reproduce the original ped animation system or foot contact. Controller
  travel follows successive root samples from the loaded walk and run clips.
  The original player shoulder matrix adjustment is applied. Turn rate, clip
  blending and mouse camera follow are bounded adaptations; exact GTA timing,
  camera collision and visual parity are still unverified. Idle now avoids
  per-frame whole-body grounding changes, but original foot placement on
  uneven ground and visual parity still need verification.
  ALT+W and W
  produce distinct walk and run travel in a GUI diagnostic. Other clips and
  runtime clothing swaps are not implemented. The standalone player preview
  remains static.
- IDE draw distances now bound loaded object selection with the original
  20-unit fade band; flags and distance alpha transitions are not yet applied.
  IPL LOD relationships are resolved, duplicate targets suppressed and
  unrepresented targets can fill gaps. Selection currently uses region focus
  instead of the exact camera position.
  The preview still uses provisional model-ID buckets and a 160-unit candidate
  margin; it checks candidate geometry bounds without fixed probe limits.
- The world preview has a provisional perspective camera with near/far clipping;
  standalone models keep an orthographic view. GTA camera follow behavior and
  world-object-level visibility selection are not implemented. `--play` follows
  CJ with a closer camera, but has no GTA camera collision or smoothing.
  Draw-group frustum culling is present.
- IMG reader validates directory and payload bounds. librw decodes selected DFF/TXD
  streams; the current Metal adapter handles 16-bit 1555/565/4444, 32-bit
  888/8888 and DXT1–DXT5 base-level textures. Other native
  texture formats and complete GTA material semantics remain unsupported.
- The project is private and local. License and redistribution preparation are outside
  the active plan; proprietary assets and executables remain untracked.
- librw has useful format/plugin code, but no Metal backend and no proven complete SA
  custom pipeline compatibility. See RENDERWARE.md for the pinned source audit.
- The accidental earlier import remains in ps2samp; do not modify that unrelated project.
- Proprietary game files, Ghidra databases, personal installation paths and local librw
  checkout remain outside tracked source (local/ is ignored).
