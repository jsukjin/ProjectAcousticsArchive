제목: Add AcousticsToolkit: C++ wrappers, test harness, and OpenMP unload crash fix

---

## Summary
- Add `AcousticsToolkit/`: C++17 wrappers for the prebuilt Project Acoustics (Triton) binaries, with Korean documentation (`README_KR.md`).
  - `HrtfEngine` (HrtfDsp.dll), `PreprocessorClient` (Triton.Preprocessor.dll), `LocalBakeRunner` (Triton.LocalProcessor.exe), `RuntimeAcoustics` (Triton.Runtime/Codec/zfp libs), plus coordinate conversion, a Triton-to-HRTF parameter bridge and a procedural scene builder.
- Tests: 47 tests, one .cpp per test (a function each), run from a single pa_tests executable. main.cpp lists them in a vector and calls each through an explicit switch (no function-pointer registration), so a test can be followed in the debugger from main -> switch -> test function -> wrapper code. Release is configured without optimization and with PDBs so breakpoints work. Includes an interactive menu (pa_tests with no arguments, or run_tests.bat) that lists every test with a description (19 unit, 9 hrtf, 10 preprocessor, 8 runtime, 1 end-to-end: mesh -> voxel/probes -> local bake -> ACE -> query -> HRTF).
- Fix a crash found while testing: unloading `Triton.Preprocessor.dll` within ~100 ms of an OpenMP parallel region crashes worker threads inside `VCOMP140.DLL`. The client now pins `VCOMP140.DLL`.

## Findings worth knowing
- `HrtfDsp.dll` does not export `HrtfEngineGetHasReverbTailRemaining`, although `HrtfApi.h` declares it (header 13 functions vs DLL 12). The wrapper treats it as optional.
- For Panner engines, `HrtfEngineGetNumOutputChannels` is not refreshed after `SetOutputFormat`, while `Process` does produce frames x format channels. `Process()` takes an explicit channel count.
- Crash root cause: workers of the OpenMP runtime spin for up to ~100 ms after a parallel region, then block. Unloading in that window returns them into unmapped code. Measured with a crash tracer and Ghidra (PDB applied). Unload delay 0 ms: crash 6/6; 150 ms or more: 0/6. Pinning VCOMP140 only: 0/8 on both success and failure paths.

## Test plan
- [x] `cmake -S AcousticsToolkit -B AcousticsToolkit/build -G "Visual Studio 17 2022" -A x64`
- [x] `cmake --build AcousticsToolkit/build --config Release`
- [x] `ctest --test-dir AcousticsToolkit/build -C Release` (47/47 tests)
- [x] e2e: 8x6x3 m room, 150 Hz, 1 probe; local bake about 5.6 s, ACE loaded, query returns finite parameters, HRTF output non-silent

## Limitations
- Windows + MSVC x64 only (`Triton.Runtime.lib` uses the MSVC C++ ABI; Release config required).
- The e2e scene is tiny; its numbers show the pipeline works, not acoustic accuracy.
- Native decompilation was limited to `Triton.Preprocessor.dll` and `vcomp140.dll`; `HrtfDsp.dll` and `LocalProcessor.exe` internals were not analyzed.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
