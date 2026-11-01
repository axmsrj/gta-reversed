# ARM64 compatibility

Host verified arm64 with Apple Clang available via Xcode. CMake 4.4.3 is present;
Ninja 1.13.2 was installed via Homebrew for native builds. Host reports macOS 27.0.1 (build 26A434).
Native deployment must target macOS 26 and arm64 explicitly.

`source/Base.h::VALIDATE_SIZE` enforces original sizes; CEntity is asserted 0x38
in `source/game_sa/Entity/Entity.h`. Native pointers/vtables change these layouts.
`source/PluginBase.h` uses __cdecl/__thiscall and integer executable addresses;
`source/game_sa/Shadows.cpp` contains x86 register assembly. Replace behavior
through targeted analysis, never by accepting these as ARM calling conventions.

Separate fixed-width disk structures from native objects. Decode little-endian
fields explicitly; bound-check counts, offsets and arithmetic. Do not disable all
size assertions or reproduce the x86 address map as a porting strategy.
The dump-only StaticRef buffer aliases unrelated globals of the same type and is
not suitable independent engine state (documented in source/Base.h).

Verified `file` reports native executable arm64. `vtool -show-build` reports macOS
minimum 26.0 and SDK 27.0. Asset tests also ran as arm64 under LLDB and ASan/UBSan.
