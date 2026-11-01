# Dependency-driven implementation plan

Implemented: separate ARM64 C++20/Objective-C++ target, Cocoa/Metal lifecycle,
logging, finite-frame smoke test and controlled shutdown. The resource component
now has owned IMG VER2 metadata, bounded entry reads and checked RenderWare chunks.
The pinned librw revision parses the real infernus DFF/TXD pair in a native probe.
See MILESTONES.md for executed verification, GHIDRA_ANALYSIS.md for binary evidence.

Current dependency chain:
1. Improve the M4 static world preview: camera navigation, LOD selection, alpha
   materials, shared resource caching, and more faithful RenderWare lighting.
2. Replace startup-only nearby selection with position-driven streaming and resource
   ownership. The text IDE/IPL and `gta3.img` binary IPL catalog is already decoded.
3. Continue extracting memory/file/timing/config foundations with owned state; inspect
   startup callees and shutdown dependencies as each component is migrated.
4. Integrate collision, animation and player control before pursuing remaining
   single-player systems.

Residual M0 findings: exact executable release/provenance, event-handler detour and
deeper RW/engine dependency closures. These
are tracked explicitly; they do not justify calling gameplay or full compatibility
complete. No broad upstream engine rewrite or multiplayer integration is authorized
by this implementation sequence. Avoid repeated whole-game analysis; use documented
targeted findings and tests for each dependency.
