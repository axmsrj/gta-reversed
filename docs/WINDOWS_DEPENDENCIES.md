# Platform dependencies

`source/CMakeLists.txt` unconditionally links ddraw, Winmm, dxguid, strmiids and
d3d9, applies /SUBSYSTEM:WINDOWS and USE_D3D9, and sets .exe/.asi suffixes.
OpenAL can replace the audio selection but does not remove graphics dependencies.
`source/app/platform/win/WinMain.cpp` uses SystemParametersInfo, DestroyWindow,
ShowCursor, Win32 handles and CRT __argv globals. Its SDL branch still needs HWND.
`source/reversiblehooks/HooksUtility.hpp::VirtualAllocForCode` allocates executable
Windows memory; omit this entire hook subsystem from native runtime.
`source/game_sa/MemoryMgr.cpp` has conditional CRITICAL_SECTION operations.
Audit transitive includes before extracting any engine source.

Reusable candidates include standard-library algorithms, JSON, spdlog, Ogg/Vorbis,
Boost headers and portable audio code, subject to technical build review.
Current CMake files contain no add_test/enable_testing setup found by inspection.
The MSVC-Wine toolchain test is outside the allowed native runtime architecture.
