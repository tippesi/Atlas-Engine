# Performance work

How to measure engine changes and what earlier measurements showed. A change is only faster when an A/B run shows it.

## Profilers

- CPU timings come from `CPUProfiler` (`src/engine/profiler/CPUProfiler.h`): `CPUProfiler::Scope scope("Name")` with a string literal (names aren't copied), `scope.Next("Name")` for consecutive sections. `GPUProfiler::Scope scope("Name")` does the same for GPU queries (begin/end of the current command list; it must end before `GPUProfiler::EndThread()`). Jobs are measured with the name of their job group, waits as "Wait for jobs". It costs ~13 ns per scope; don't put scopes into loops over many items.
- Both profilers live in `src/engine/profiler/` (namespace `Atlas`) and return the shared `Profiler::Query` (name, time, calls, children; same names are merged). `ImguiExtension::ProfilerPanel` shows both (CPU and GPU tab, editor profiler window and demo), bridge jobs write `cpuThreads` next to the GPU `threads` into `profile.json`.
- `HitchRecorder` (`src/engine/profiler/HitchRecorder.h`) keeps the CPU profile of frames over `minimumFrameTime` and `relativeThreshold` times the average; the game and the editor log them with `--log-hitches [ms]`.
- A `JobSystem::Wait` on the main thread helps with any job of that priority pool and can run a long one (e.g. a navigation tile build) to the end: the UI must not wait on medium/low jobs every frame.

## Measuring

GPU timings come from the `profile` section of a bridge job (`profile.json`, milliseconds per profiler query). `AtlasBridge bench` runs the A/B procedure below for builds or shader directories (`src/bridge/README.md`, Measuring).

- Make runs deterministic before measuring: pause the scene right away (`{ "t": 0.0, "pause": true }`, after a night switch or similar transition if needed), so physics, wind and scripts stand still, set the sun with `settings.sun`, and disable auto exposure (`postProcessing.autoExposure.enable: false` with a fixed `exposureCompensation`). While paused, auto exposure doesn't adapt; only enable it when measuring the exposure passes.
- Check the noise first: two runs of the same build have to produce the same image and pass timings within a few microseconds. Some passes stay noisy even then, judge them only over several runs.
- Measure at 1920x1080 over 240 frames, with the window shown. Make sure the scenario runs the changed code: no checked-in scene uses a skybox probe (add one with `"sky": { "probe": { "cubemapPath": "environment.hdr" } }`), render targets use a 0.75 scaling factor by default, sponza has a point light with a cube shadow, Kingdom has shadow cascades, terrain shadows and clouds, and `N` switches it to night with the lanterns.
- Paused timings repeat within microseconds, so one interleaved old/new pair with one or two shots is the default.
- Always interleave the builds (old, new, old, new, ...) instead of comparing with runs from earlier in the session. For C++ changes keep a copy of the old executable next to the new one (e.g. `bin/RelWithDebInfo/AtlasBridge_base.exe`) and delete it afterwards.
- The baseline only differs by the change being measured. Unrelated uncommitted work, including the user's, stays in both builds.
- CPU gains are hidden when the GPU limits the frame. Keep all effects on at really low settings (DDGI with 11x11x11 probes, half resolution effects, below 50% internal resolution).
- Shaders are compiled at runtime, so shader variants can be swapped between runs without a rebuild. The SPIR-V cache in `data/.cache/` is keyed by shader path and macros and reused while the shader and its includes are older than the cache entry: set the modification time of swapped files to now (copying keeps the old time). It's only written when the application closes. Other sessions use it too, so skip it in the compiler instead of moving it.
- Compare the frame total (`Render scene`) and the changed passes as mean and spread over the rounds. Passes that weren't changed can still move because overlapping GPU work is attributed differently; don't credit or blame the change for that without evidence.
- Verify the images with `--compose` and compare the pixel difference between old and new with the difference between two runs of the same build. When barriers or pipelines change, run `--validation` once with each build and compare the message ids.
- Report per scenario: old and new in ms with spread, the number of rounds, and which results are inconclusive.

## Known costs

- Shader changes affect register pressure: computing values early that stay live across a texture-heavy loop can make the shader slower. Measure alternatives; a minimal early-out often beats moving code around.
- Fields read per mesh, material or draw count as cold reads: with thousands of meshes one cold cache line per mesh showed up as +0.07 ms. Keep them next to fields those loops read anyway.
- Upload all layers of an array with one `MemoryTransferManager::UploadImageData` call: every call transitions the whole image and regenerates its mips (256 per-layer uploads made a single impostor depth array cost 25-70 ms).
- Common cheap wins: dispatch sizes have to come from the image a pass writes (half resolution targets, `GetScaledWidth()` instead of `GetWidth()`), bounds checks use `>=` against `imageSize`, expensive sampling (shadows, ray marching) gets an early-out when its result is multiplied by zero, and work on static data (e.g. probe filtering) shouldn't run every frame.
