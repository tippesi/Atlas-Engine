# AtlasEngineCapture

Command line tool to look at, measure, play, check and edit scenes without the editor. It is
meant for automated verification, e.g. by coding agents: every run produces images, JSON
reports and an exit code.

The tool only uses the public engine API and lives in its own CMake subproject. Build it with
`-DATLAS_CAPTURE=ON`:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DATLAS_CAPTURE=ON
cmake --build build --target AtlasEngineCapture
```

It renders with the real GPU, exactly like the editor viewport (same `MainRenderer::RenderScene`
path). SwiftShader/headless builds don't produce correct images.

By default a window shows the rendered frames with a status line and the latest output below them
and plays in real time; it closes when the job is done. `--hidden` (`"showWindow": false`) renders
into a hidden window and plays as fast as possible, which suits automated runs. Keyboard input into
a shown window reaches the scene like the job's key events, so don't type into it while it runs.

## Quick start

```powershell
# One picture with the scene camera (or an overview if the scene has none)
bin/RelWithDebInfo/AtlasEngineCapture.exe --scene scenes/sponza.aescene

# Several views of an entity plus pass images
bin/RelWithDebInfo/AtlasEngineCapture.exe --scene scenes/sponza.aescene --focus Player --views front,right,low,top --passes albedo,normal

# Scene summary (entities, transforms, bounds, settings) as JSON
bin/RelWithDebInfo/AtlasEngineCapture.exe --scene scenes/Kingdom.aescene --inspect

# A job file describes everything else
bin/RelWithDebInfo/AtlasEngineCapture.exe src/capture/examples/kingdom_gate_guards.json

# Compare images of different runs, e.g. before and after a shader change (no scene is loaded)
bin/RelWithDebInfo/AtlasEngineCapture.exe --compose out.png --title "Clouds" before=a/sky.png after=b/sky.png
```

Run `--help` for all command line options. Results are written to `bin/capture/<job name>/`
(or `--out`): images, `report.json`, `log.txt` and the other files listed below.

Exit codes: `0` passed, `1` a check or expectation failed, `2` fatal error (invalid job, scene
didn't load, output directory in use, window closed early, ...), `3` crash. The last line is always
`Status: <status> (<reason>)` and `report.json` has the same `status` and `reason`. Crashes print a
stack trace and write a report with the status `crashed`.

## How a run works

1. The scene is loaded; the tool waits until all meshes are loaded (or failed to load, which is
   reported as a warning), the terrain is loaded and the ray tracing data is built.
2. `settings` are applied, then `inspect`, `queries` and `edit` run.
3. Each shot renders `settleFrames` frames before it is captured, such that temporal effects
   (TAA, DDGI, clouds) converge.
4. With `play`, the simulation runs with a fixed time step (`fixedDt`) for `duration` seconds while
   the timeline is processed.
5. Checks run once for static jobs and every `checks.interval` seconds while playing.

The scene file is never modified unless `edit` is used.

## Server mode

Loading a large scene takes most of the time of a short job. `--serve <directory>` keeps the tool
running and processes the commands written to the directory as JSON files, in the order of their file
names. A processed command is moved to `<directory>/done/` and its result is written to
`<directory>/<command>.result.json` (`status`, `seconds`, the loaded `scene`, for jobs also `exitCode`,
`outputDirectory` and `report`). Options given after `--serve` (e.g. `--hidden`) apply to every job.

```jsonc
{ "command": "loadScene", "scene": "scenes/Kingdom.aescene", "timeout": 300, "camera": { ... } }
{ "command": "run", "job": "src/capture/examples/kingdom_night.json", "args": ["--hidden"] }
{ "command": "run", "job": { "shots": [{ "name": "overview", "camera": "overview" }] } }   // inline, uses the loaded scene
{ "command": "reloadScene" }   // back to the state of the file
{ "command": "unloadScene" }
{ "command": "stop" }
```

A job whose `scene` is the loaded scene runs on it without loading; a job for another scene fails
until the loaded scene is unloaded. Without a loaded scene a job loads its scene and unloads it
afterwards, like a normal run. Each job starts with the scene settings of the file, out of play mode,
but otherwise jobs work on the scene in memory: edits, scripts attached by a job and the state left by
playing remain until `reloadScene`. Jobs that change the resolution wait for the GPU first.

```powershell
bin/RelWithDebInfo/AtlasEngineCapture.exe --serve bin/capture-server
# e.g. from a script or another shell
'{ "command": "loadScene", "scene": "scenes/sponza.aescene" }' | Out-File -Encoding ascii bin/capture-server/01_load.json
```

## Pipelines

A pipeline builds a scene with a sequence of edit jobs, each working on the result of the one before.
`--pipeline <file>` runs them in one process and keeps the state after each step as a snapshot: the
scenes the steps saved, the materials they changed and the `files` listed by the pipeline (e.g. files
written by edit operations). Each step has a key from its job file, its `inputs` and the keys of the
steps before it. A run starts at the first step whose key changed, restores the snapshot of the step
before it and runs the remaining steps, so changing a late step doesn't run the early ones again.

```jsonc
{
    "name": "sponza_pipeline",
    "steps": [
        "sponza_pipeline_1.json",
        { "name": "move", "job": "sponza_pipeline_2.json", "inputs": ["../scripts/used_by_step_2.lua"] }
    ],
    "files": ["prefabs/x.aeprefab"],               // further files that belong to the state, relative to data/
    "snapshots": "../../../bin/capture/sponza_pipeline/snapshots"   // default: bin/capture/<name>/snapshots
}
```

Job files and inputs are relative to the pipeline file. `--from <step>` runs again from a step, `--force` runs all
steps; options like `--hidden` apply to every step. If all steps are up to date, the files of the last
snapshot are restored in case they were changed since. A step that reads the scene the previous step
saved continues with it in memory. A failed step stops the pipeline; its snapshot and those of the
following steps are removed. Snapshots store each file content once, steps that don't change a file
share it. Steps don't back up the scenes they overwrite, the snapshots keep the previous states.
`pipeline.json` next to the snapshots lists the result of each step. See
`examples/sponza_pipeline.json` and `examples/kingdom_expanded_pipeline.json`.

## Job file

All keys are optional except `scene`. Unknown keys are rejected to catch typos.

```jsonc
{
    "scene": "scenes/Kingdom.aescene",   // relative to data/ or the launch directory
    "name": "kingdom",                     // default: job file name
    "out": "capture-out/kingdom",          // default: bin/capture/<name>
    "resolution": [1280, 720],             // output resolution of the captures
    "renderScale": 0.75,                   // render target scale, default: engine default (0.75)
    "settleFrames": 60,
    "loadTimeout": 300,                    // seconds
    "fixedDt": 0.016667,
    "showWindow": true,                    // false or --hidden: no window, no real time playback
    "pathTrace": false,                    // render with the path tracer, it accumulates over the settle frames

    "settings": { "fog": { "density": 0.02 } },     // merged into the scene settings (see below)
    "variants": [ { "name": "noFog", "settings": { "fog": { "enable": false } } } ],

    "inspect": { "file": "scene.json", "filter": ["*Gate*"], "detail": ["Player"] },
    "queries": [ ... ],
    "edit": { ... },
    "shots": [ ... ],

    "play": true,
    "duration": 5.0,
    "observer": { ... camera ... },
    "scripts": [ "test.lua", { "script": "scripts/x.lua", "entity": "Player" } ],
    "timeline": [ ... ],
    "strips": [ ... ],
    "track": { "entities": ["Kingdom Guard*"], "interval": 0.25 },
    "checks": { ... },
    "profile": { "frames": 64 }
}
```

Entities are addressed by name patterns with `*` and `?` wildcards or by id (`"#123"`). When a
pattern matches a parent and its children, only the topmost matched entities are used.

### Settings and variants

`settings` patches the scene's feature settings with JSON merge semantics. The keys and fields
are the ones of the scene serializer: `sky` (including `clouds` and `atmosphere`),
`postProcessing`, `wind`, `fog`, `irradianceVolume`, `reflection`, `rtgi`, `ssgi`, `sss` and `ocean`. The
current values are in the `settings` section of an `--inspect` output. On the command line use
`--set fog.density=0.02`. Patching `ocean` adds an ocean to a scene without one, e.g.
`{ "ocean": { "translation": { "x": 0, "y": 0, "z": 0 }, "N": 512 } }`. Changing `N`, `size` or `LoDCount`
recreates the simulation. Two additional keys:

- `sun`: the main directional light, `{ "elevation": 12, "azimuth": 10 }` in degrees (azimuth 0 places
  the sun on the +Z side) or `"direction"`, plus `"color"` and `"intensity"`. Clouds, fog and
  materials have to be judged with front, side and back light.
- `materials`: patches loaded materials whose path matches a pattern, with the keys of the
  `.aematerial` files, e.g. `{ "*M_PolygonFantasyKingdom_01_A*": { "metalness": 0.0, "roughnessMapPath": null } }`.
  It only changes the running scene, use the `material` edit operation to change the files.

Each entry of `variants` renders all shots again with additional settings. Besides one folder
per variant, a `<shot>_variants.png` sheet compares the variants side by side.

### Render scale and upscaling

The scene renders into a render target scaled by `renderScale` (`--render-scale`), which is
upscaled to the job resolution like in the editor: with FSR2 if `postProcessing.fsr2` is set (the
default), otherwise with TAA if `postProcessing.taa.enable` is set. A variant can override the
scale, e.g. to compare upscalers and quality modes:

```jsonc
"variants": [
    { "name": "native", "renderScale": 1.0, "settings": { "postProcessing": { "fsr2": false } } },
    { "name": "fsr2Quality", "renderScale": 0.67, "settings": { "postProcessing": { "fsr2": true } } },
    { "name": "fsr2Performance", "renderScale": 0.5, "settings": { "postProcessing": { "fsr2": true } } }
]
```

The log and `report.json` contain the render scale. Pass images (`passes`) show the render
target textures at their scaled resolution, stretched to the output resolution.

### Cameras

```jsonc
"scene"                                                         // the scene's main camera
"overview"                                                      // frames the whole scene
{ "position": [x, y, z], "lookAt": [x, y, z] }                  // or "yaw"/"pitch" in degrees
{ "entity": "Gate*", "view": "front", "padding": 1.25 }         // focus, framed by the entity bounds
{ "entity": "Player", "offset": [0, 2, -6], "local": true }     // follow (while playing)
```

Common keys: `fov` (60), `near` (0.1), `far` (2000). Manual cameras with `"groundRelative": true`
take the heights of `position` and `lookAt` above the first surface below them, seen from above the
scene, which suits terrains and open scenes (indoors that is the roof). Focus cameras also accept `azimuth`/`elevation`
in degrees, `distance`, `local` (rotate the view with the entity) and `avoidOcclusion` (default
true): if geometry hides the entity, nearby angles are tried and the one from which most points of
the entity bounds are visible is used. Moving closer is the last resort and only happens while
the camera stays outside of the entity bounds.

Views: `front` (camera on the +Z side), `right` (+X), `back`, `left`, `top`, `iso`, and `low`/`lowSide`,
which are almost at ground level. `low` views are the easiest way to see whether something floats.

### Shots

```json
{ "name": "gate", "camera": { "entity": "Gate*" }, "views": ["front", "right", "low"], "passes": ["normal", "depth"] }
```

A shot with several views writes `<name>_<view>.png` and a `<name>_views.png` sheet. `passes`
writes additional debug images and a `<name>_passes.png` sheet: `albedo`, `normal`,
`geometryNormal`, `roughness` (roughness/metalness/AO), `emissive`, `depth`, `velocity`, `stencil`,
`materialIdx`, `reflection`, `volumetric`, `clouds`, `sss` and `gi`.

### Playing: observer, timeline, strips and scripts

While playing, the `observer` camera (default: the scene camera) is used for live captures.
During the scene update it is the main camera, so culling, shadows and terrain LODs fit the
captured view. Scripts that access their camera through `GetThisEntity():GetCameraComponent()`
are not affected. A focus or overview observer is framed once when playing starts (a moving
target would make the camera jump), `"dynamic": true` reframes it every frame.

Like the editor in play mode (both use `Input::PlayerInput`), keyboard input moves players (`W`,
`A`, `S`, `D`, `Left Shift` for running, `Space` for jumping) relative to the direction of their
camera. The mouse isn't used, use `look` events to turn them.

Timeline events, each with a time `t` in seconds:

```jsonc
{ "t": 0.0, "teleport": { "entity": "Player", "position": [x, y, z], "yaw": -90 } }   // yaw/pitch optional, entities without camera only turn
{ "t": 0.0, "look": { "entity": "Player", "yaw": -90, "pitch": 0 } }   // camera direction, yaw 0 looks along +Z
{ "t": 0.5, "key": "E" }                                        // press: down, released 2 frames later
{ "t": 0.5, "key": { "key": "W", "action": "down" } }          // down / up / press (+ "duration")
{ "t": 1.0, "capture": { "name": "now" } }                     // live capture with the observer
{ "t": 1.0, "capture": { "name": "side", "camera": { ... }, "passes": [...] } }   // still
{ "t": 2.0, "expect": { "entity": "Crate", "property": "bottom.y", "op": "<", "value": 0.1 } }
{ "t": 2.0, "log": "message" }
{ "t": 2.5, "animation": { "entity": "Fox", "animation": "Walk", "model": "skinning/Fox/Fox.gltf", "blendTime": 0.4 } }   // or an .aeanimation file
{ "t": 3.0, "pause": true }          // pauses scripts, physics and players, the job time continues
{ "t": 4.0, "pause": false }         // resumes
{ "t": 5.0, "resetScripts": true }   // discards non-permanent script environments like leaving play mode does
```

Key names are SDL key names (`E`, `Space`, `Left Shift`, `Up`, ...).

Instead of a time, an event can wait for a condition. Use it when the timing depends on the
simulation, e.g. to jump at an edge or to react to a script:

```jsonc
// Same properties and operators as expectations, all matching entities have to fulfill it
{ "when": { "entity": "Player", "property": "position.z", "op": ">", "value": 29.5 }, "key": "Up" }
// A log message of the engine or a script, without * or ? it matches messages containing the text
{ "when": { "log": "Closing castle gate" }, "delay": 0.5, "capture": { "name": "closing" } }
// t is the earliest time the condition is checked, optional events may never happen
{ "t": 2.0, "when": { "log": "Gate*opened" }, "optional": true, "expect": { ... } }
```

Each conditional event happens once, in the frame its condition is met (plus `delay`). One that
never happens fails the job, unless it is `optional`. `report.json` lists when each condition was met
(`conditionalEvents`). See `examples/kingdom_conditions.json`.

Expectation properties: `exists`, `count` (number of matching entities, e.g. to find duplicates
after a script reset), `position.x/y/z`, `bottom.y`, `top.y` (of the bounds), `yaw`, `distanceTo`
(with `point`) and `gapToGround` (entities without a mesh are skipped). Operators: `<`, `<=`, `>`, `>=`, `==`, `!=` (with
`tolerance`). All matching entities have to fulfill an expectation.

Live captures show the frame that simulates the step from their time on, so motion vectors and
temporal effects are those of the running scene. Captures with their own camera are stills: the
scene is paused for `settleFrames` frames, then the observer continues. Stills have no motion.

`pause` checks that a paused scene stays still (combine it with expectations or a strip) and
that scripts continue with their state when it resumes. Scripts with `permanentExecution` keep
running while the scene is paused or outside play mode.

`strips` capture a sequence and compose a labeled sheet (`<name>.png`), which shows a whole
movement in a single image:

```json
{ "name": "gate_opening", "from": 0.0, "to": 3.0, "every": 0.5, "columns": 4 }
```

Without `camera` the observer is used (live), with a camera every frame is a still.

`scripts` attaches Lua scripts for the duration of the run, either to new entities or to
existing ones (`entity`). Script paths are relative to the job file or to `data/`. This is a
convenient way to test script code or Lua bindings, see `examples/raycast_test.lua`.

`track` records the positions of entities and writes `trace.json` with a summary per entity
(start, end, distance travelled, minimum/maximum height).

`resetScripts` checks that non-permanent scripts recover after leaving play mode discards their
environments, e.g. when the simulation is stopped without restoring the scene. Unlike `pause`,
it discards script state; permanent scripts keep running. State that has to survive belongs to
the scene, e.g. marker entities.

### Overlays

```jsonc
"overlay": {
    "bounds": ["*Portcullis*"],       // bounds of entities (yellow)
    "players": true,                  // figures for player entities, including those without meshes (cyan)
    "trails": true,                   // paths of tracked entities (magenta)
    "queries": true,                  // query results: raycasts, ground hits, paths, picks (green/red)
    "markers": [{ "position": [x, y, z], "color": [1, 0, 0], "size": 0.5 }],
    "axes": [[x, y, z]],              // X red, Y green, Z blue
    "depthTest": false                // drawn on top of everything by default
}
```

The lines are drawn with the engine primitive batches into every capture (`--overlay` enables
the defaults). They show what can't be seen otherwise: invisible players, the path a character
walked, where a ray hit or which part of a route is blocked.

### Checks

```jsonc
"checks": {
    "grounded": { "entities": ["Kingdom Guard*"], "maxGap": 0.15, "maxSink": 0.3 },
    "penetration": { "entities": ["*Portcullis*"], "against": ["*"], "margin": 0.05, "minTriangles": 1 },
    "logErrors": true,                    // or { "ignore": ["*music.wav*"] }
    "invalidTransforms": true,
    "interval": 0.1
}
```

- `grounded`: casts a ray down from just above the entity (ignoring the entity and its children)
  and compares the bottom of its bounds with the hit. Fails if the gap exceeds `maxGap` or the
  entity sinks deeper than `maxSink`. Entities whose hierarchy has no loaded meshes are skipped. With `"surfaces":
  ["*Ground*", "*Path*"]` the surface below also has to match, e.g. guards walking over hay bales
  are grounded, but not on a valid surface.
- `penetration`: counts the triangles of other meshes (matching `against`, by name or a parent's
  name) intersecting the entity bounds shrunk by `margin`. Intended intersections at the start (a gate
  in its frame) are the baseline; only additional intruding triangles fail.
- `logErrors`: engine errors fail the job, `ignore` takes patterns of known errors.
- `invalidTransforms`: NaN or infinite transforms.

### Queries

Spatial queries run once after loading and are written to `queries.json`. `raycast` and `pick`
hit what is visible (meshes with triangle precision, terrain), `raycast` physics bodies only with
`"bodies": true`. `ground`, `entity`, `clearance`, `path` and the `grounded` check also query the
physics world: bodies are hit with their collision shapes, meshes without a mesh shape body (e.g.
grass) and the terrain directly. Players are never hit.

```jsonc
{ "type": "ground", "x": -108.0, "z": -21.0, "from": 20.0 }     // height below a point
{ "type": "raycast", "origin": [...], "direction": [...], "ignore": ["Player"], "maxDistance": 100 }
{ "type": "entity", "entity": "Gate*" }        // position, yaw, bounds, ground below, gap to ground
{ "type": "around", "position": [...], "radius": 5 }            // meshes nearby, sorted by distance
{ "type": "clearance", "entity": "*Portcullis*", "direction": [0, 1, 0], "distance": 10 }
{ "type": "path", "points": [[x, y, z], ...], "maxStep": 0.3, "radius": 0.4, "spacing": 0.5 }
{ "type": "pick", "camera": { ... }, "pixels": [[x, y], ...] }   // what is visible at pixels of a shot
{ "type": "terrain" }   // extents, resolution, material slots and usage, height range, connected holes with rim heights
{ "type": "terrainSample", "points": [[x, z], ...] }   // or "min", "max", "step": height, slope, material, hole
{ "type": "terrainMap", "file": "map.png", "min": [x, z], "max": [x, z], "size": 2048, "grid": 100, "contour": 5,
  "smallMeshes": false, "meshes": true, "highlight": ["Dungeon"],
  "markers": [{ "position": [x, z], "radius": 10, "label": "hole", "color": [1, 0, 1] }],
  "paths": [{ "points": [[x, z], ...], "color": [1, 0.9, 0.1] }] }
{ "type": "route", "from": [x, z], "to": [x, z], "via": [], "cell": 2, "margin": 60, "maxSlope": 30, "maxStep": 0.6,
  "clearance": 1, "avoidMaterials": ["*Water*"], "avoid": [], "walkOn": ["*Ground*", "*Bridge*"],
  "ignore": ["Grass*"], "map": "route.png" }
{ "type": "buried", "entity": "Dungeon", "margin": 1.0 }   // meshes reaching above the terrain or lying below a hole
```

`clearance` casts a grid of rays from the face of the entity bounds into the direction and
returns the sampled free distance and the first blocker. Obstacles between the rays can be
missed; this is not a shape sweep. Use it before moving something.

`path` samples the ground along a polyline and reports height changes larger than `maxStep`
and ray hits at knee and chest height at the center and offsets of `radius`. It is a route
diagnostic, not a character-controller simulation.
Validate routes for characters with it before putting them into a script.

`pick` answers "what is that in the image": for pixels of a camera (the same camera spec as in a
shot, with the job resolution) it returns the entity and the material of the hit triangle.

`path` points can be `[x, z]` with `"snap": true`, they then start at the terrain height.

The terrain queries load all terrain cells and keep them loaded for the rest of the run. Positions
are `[x, z]` (or `[x, y, z]`), material slots are the indices of the terrain materials, which
`terrain` lists. `terrainSample` is the quickest way to find flat ground, slopes and the shape of a
site before placing anything. `terrainMap` draws a top down map into the output directory: terrain
materials with hill shading and contour lines, holes in magenta, mesh footprints (buildings red, trees
green, rocks grey), highlighted hierarchies in cyan, a labeled grid, markers and paths.

`route` searches a walkable route with A* over a grid around the stops. A cell is blocked by holes,
slopes above `maxSlope`, avoided materials or entities, objects lying on the ground (unless their
name or a parent name matches `walkOn`) and obstacles at knee and chest height. `clearance` keeps the
route that many cells away from blocked cells. The result is a polyline with heights, simplified by
line of sight. `map` writes the grid: free cells green, cells near obstacles dark green, blocked cells
by reason (black: no ground, orange: slope, blue: avoided, grey: object on the ground, red: obstacle)
and the route in yellow. Without a route the error names the entities that block the most cells, a
stop inside a closed area (e.g. fenced in) is a common reason. Overlays draw routes in yellow. Routes
need a terrain.

`buried` lists the meshes of a hierarchy that reach above the terrain or lie below a hole, e.g. to
check that a level placed below the terrain only shows at its entrance.

### Profile

`"profile": true` (or `{ "frames": 64 }`, default 32) averages the GPU profiler timings over the
last frames of the run, prints the passes that take at least 0.05 ms and writes all of them to
`profile.json`. Play the scene for long enough that the last frames are representative. Work that
only runs on changes (e.g. the atmosphere tables) can be forced every frame with a script to
measure it.

### Validation

`--validation` enables the Vulkan validation layers with GPU assisted, synchronization and best
practices validation (requires the Vulkan SDK). The run is much slower. The messages are grouped by
their id in `validation.json`, errors first, and the number of distinct errors, warnings and
performance warnings is added to `report.json`. Validation errors are also engine log errors and
fail the `logErrors` check. The layer stops reporting a message id after 10 occurrences, so `count`
is at most 10. The option only exists on the command line, since the validation layers have to be
enabled before the job is read.

### Editing

```jsonc
"edit": {
    "output": "scenes/Kingdom.edit.aescene",   // default: <scene>.edit.aescene next to the input
    "overwrite": false,
    "operations": [
        { "op": "snapToGround", "entity": "Barrel_12", "offset": 0.0 },
        { "op": "translate", "entity": "#4711", "delta": [0, -6, 0] },           // "space": "world" | "local"
        { "op": "setTransform", "entity": "Gate", "position": [x, y, z], "rotation": [0, 90, 0], "scale": [1, 1, 1] },
        { "op": "rotate", "entity": "Statue", "yaw": 45 },
        { "op": "rename", "entity": "Entity 7", "name": "Crate" },
        { "op": "setVisible", "entity": "Debug*", "visible": false, "all": true },
        { "op": "duplicate", "entity": "Lantern", "name": "Lantern 2", "position": [x, y, z] },
        { "op": "delete", "entity": "Old*", "all": true },
        { "op": "material", "material": "kingdom/materials/X.aematerial", "set": { "metalness": 0.0, "roughnessMapPath": null } },
        { "op": "add", "name": "Ramp", "mesh": "skate/models/kicker.gltf", "body": true, "position": [x, y, z], "rotation": [0, 180, 0] },
        { "op": "add", "name": "Ramp Light", "position": [x, y, z],
          "light": { "type": "spot", "color": [1, 0.2, 0.6], "intensity": 180, "radius": 18, "direction": [0, -1, 0], "innerAngle": 20, "outerAngle": 35 } },
        { "op": "add", "name": "Game", "script": "skate/skate.lua" },
        { "op": "add", "name": "Blades", "parent": "Windmill", "mesh": "...", "localPosition": [0, 0.5, 0.55], "visible": false },
        { "op": "add", "name": "Fox", "mesh": "skinning/Fox.glb", "animation": "Run", "animationSpeed": 1.0, "animationTime": 0.0 },
        { "op": "scatter", "group": "Forest", ... },                                     // see below
        { "op": "clearArea", "entities": ["Grass*"], "area": { "center": [x, z], "radius": 20 } },
        { "op": "terrainCopy", "path": "terrains/x/x.aeterrain", "overwrite": false },
        { "op": "terrain", "brushes": [ ... ] },                                         // see below
        { "op": "makePrefab", "scene": "scenes/Demo.aescene", "output": "prefabs/Dungeon.aeprefab", "name": "Dungeon",
          "exclude": ["Atmospheric Fog"], "scripts": false },
        { "op": "prefab", "prefab": "prefabs/Dungeon.aeprefab", "name": "Dungeon", "parent": "Level", "position": [x, y, z], "rotation": [0, 55, 0] },
        { "op": "meshCopy", "mesh": "a.aemesh", "output": "b.aemesh", "invertNormals": true, "set": { "cullBackFaces": false }, "assign": ["Grass*"] },
        { "op": "meshSettings", "meshes": "dungeon/meshes/*.aemesh", "set": { "cullBackFaces": false } },
        { "op": "materialCopy", "material": "a.aematerial", "output": "b.aematerial", "set": { "emissiveIntensity": 0 }, "remove": ["emissiveMapPath"] },
        { "op": "meshMaterials", "meshes": ["b.aemesh"], "replace": { "a.aematerial": "b.aematerial" } }
    ]
}
```

`add` creates a new entity below the root (or `parent`) with any combination of a mesh, a light and a
script. Model files other than `.aemesh` (glTF, FBX, ...) are imported first, next to the source into
`meshes/` and `materials/`, because scenes only reference `.aemesh` files. `"body": true` adds a static
mesh shape, such that the piece collides and is found by physics queries. Lights are `point` or `spot`
lights at the entity origin (`radius`, `intensity`, `color`, `volumetric` (off by default), `shadow`;
spots also `direction` in entity space and the cone half angles in degrees). Spot lights with `shadow` currently
stay dark when created this way.
`animation` adds an animation component to a skinned mesh: either an `.aeanimation` file or the name
of an animation in the model file, whose animations are then imported into `animations/` next to it
(`animationSpeed`, `animationTime` in seconds).

`add`, `prefab` and `scatter` put new entities below the root of the scene (or `parent`); scenes
without a root get top level entities. `localPosition` places an added entity or prefab (with
`rotation` and `scale`) in the space of its parent (with `"parentJoint": <index>` in the space of a joint of an
animated parent, which the entity then follows), `"visible": false` adds a hidden mesh, e.g. one a
script shows.

`edit.dryRun` applies the operations and runs the checks and shots on the edited scene without
saving it or edited materials. Operations that write their own files (`terrainCopy`, `meshCopy`,
`meshSettings`, `materialCopy`, `meshMaterials`, `makePrefab`) still do. `edit.queries` run on the
edited scene once its transforms and bounds are updated and are written to `edit_queries.json`,
e.g. to verify placements.

Asset operations, for meshes and materials that need different settings in a scene:

- `meshCopy` copies a mesh file, optionally inverts its normals, changes the mesh settings `set`
  (`invertUVs`, `cullBackFaces`, `vegetation`, `castShadow`) and assigns the copy to the meshes of the
  selected hierarchies that use the original. The original file stays untouched.
- `meshSettings` changes these settings in all mesh files matching the pattern, after keeping a
  `<file>.backup` once. It changes shared assets: every scene using the meshes is affected.
- `materialCopy` copies a material file and changes (`set`, colors as `[r, g, b]`) or removes (`remove`)
  its properties. `meshMaterials` replaces materials in mesh files (`replace`: old path to new path),
  e.g. in copies made by `meshCopy`.
- `makePrefab` turns the root of a scene file (MessagePack or JSON) into a prefab. Players, cameras,
  directional lights and excluded names are left out, scripts with `"scripts": false`. With
  `"area": { "center": [x, y, z], "radius": r }` only the mesh and light entities inside the sphere are
  kept (those whose mesh matches `includeMeshes` and not `excludeMeshes`), flattened and relative to the
  center, e.g. to reuse a vignette or a building of another scene. Meshes of other scenes are often
  split into parts whose offsets are lost when placed one by one, a prefab keeps them.

`terrain` changes the terrain with brushes, radii in meters (at most about one terrain cell). Terrain
files can be shared by several scenes, so it only works on a copy made by `terrainCopy` or with
`"overwriteTerrain": true`. The physics height field is rebuilt after the brushes, the lower levels
of detail are baked and the terrain is saved together with the scene.

```jsonc
{ "type": "flatten", "position": [x, z], "radius": 12, "height": 11.8, "strength": 1, "falloff": 10 }
{ "type": "raise", "position": [x, z], "radius": 20, "amount": 6 }   // gaussian mound, negative amounts dig
{ "type": "smooth", "position": [x, z], "radius": 20, "strength": 0.5, "iterations": 2 }   // or "points"
{ "type": "hole", "position": [x, z], "radius": 3 }
{ "type": "material", "position": [x, z], "radius": 10, "slot": 5 }
{ "type": "path", "points": [[x, z], ...], "width": 4, "slot": 5, "smooth": 0.25 }
```

Brushes don't move what stands on the terrain: clear and place things after changing the ground,
e.g. `clearArea` the grass of a site, then the brushes, then `scatter` it again.

`scatter` places mesh instances inside `area` (`center`/`radius`/`innerRadius`, `min`/`max`, `polygon`
or `path`/`width`, all in x/z) and outside of the `exclude` areas into a new group, deterministically for a `seed`:

```jsonc
{ "op": "scatter", "group": "Pines", "parent": "Nature", "append": false, "seed": 11, "required": false,
  "count": 2400, "attempts": 96000, "area": { "polygon": [[x, z], ...] }, "exclude": [ ... ],
  "materials": [0, 5], "slope": [0, 32], "height": [-20, 100], "castFrom": 5.0,
  "noise": { "scale": 110, "threshold": -0.35, "softness": 0.35 },
  "spacing": 0, "clearance": 0.5, "ignore": ["Grass*"], "maxBlockerSize": 120, "onMeshes": false,
  "items": [{ "mesh": "...", "weight": 3, "scale": [0.85, 1.3], "yaw": [0, 360], "tilt": 3, "pitch": 0, "align": 0,
              "sink": 0.15, "pivotOnGround": false, "radius": 2.0, "body": "capsule", "bodyRadius": 0.35, "bodyHeight": 5,
              "name": "Lantern", "light": { "offset": [0, 1.5, 0], "color": [1, 0.6, 0.3], "intensity": 2, "radius": 9 } }] }
```

- On a terrain the candidates are filtered by terrain `materials`, `slope` and `height`, and the ground
  has to be the terrain itself unless `onMeshes` is set (ignored hierarchies don't count). Scenes
  without a terrain use the first surface below `castFrom` (default: above the scene), e.g. a floor
  below a roof.
- The simplex `noise` mask keeps instances where the noise is above `threshold` (with a linear ramp of
  `softness`), which creates clusters and clearings.
- Instances keep `spacing` (or the sum of their footprint `radius`) to each other and `clearance` to
  existing meshes, except `ignore`d hierarchies, the mesh they stand on and meshes larger than
  `maxBlockerSize`. -1 disables the clearance test.
- The lowest point of the mesh bounds (after `pitch`, a fixed rotation around x, e.g. for fallen
  objects) is put on the ground, `pivotOnGround` puts the pivot there instead. `align` tilts instances
  towards the ground normal, `tilt` randomly, `sink` moves them into the ground relative to their scale
  (negative values lift them).
- Bodies are `"mesh"` (static mesh shape) or `"capsule"` (a feet aligned trunk, the crown stays passable).
- `name` replaces the instance names (`<group> <mesh> <index>`), `light` adds a child with a light like the
  one of `add`, at `offset` in the space of the instance.
- `append` adds to an existing group. `required` fails the job when nothing could be placed. The
  result lists placed instances per mesh and rejections per reason; scatters with a million attempts
  or more print their progress.

`material` patches the loaded material (`null` removes a map, paths are relative to `data/`). Once
the checks passed, the material file is written by the engine, which keeps references inside a
package relative, and the original is kept as `<file>.backup`. Edits which only change materials
don't rewrite the scene. `"saveUnchanged": true` saves it anyway, e.g. to store the job `settings` or data that was
migrated while loading.

Operations selecting existing entities need exactly one topmost match unless `"all": true` is set. `position` is in
world space and `localPosition`/`rotation`/`scale` in the parent space; the parent transform is
taken into account.

After applying the operations the tool writes `edit_diff.json` (changed, added and removed
entities based on selected inspection fields), runs the checks and only saves if they pass.
The diff does not compare all component data or scene settings. The scene is saved through
`Atlas::Serializer` without touching dependencies, then loaded again to verify it. The input
scene is never overwritten without `overwrite`, and an overwritten file is backed up first
(`<file>.backup-<timestamp>`). `edit` can't be combined with `play`, because scripts create
runtime entities that would end up in the saved scene: verify an edit with a second job on the
saved scene.

## Output files

Every run gets an id (`runId` in the output and the report). Before a run, the files listed in the
previous `report.json` of the output directory are removed and `report.json` is replaced by one with
the status `running`, so results of an earlier run never look like results of this run, even if it
crashes or is killed. Other files are kept. JSON files are written to a temporary file and renamed.

`capture.lock` marks an output directory as used while a run writes into it. A second run with the same
directory stops with an error that names the running process; use `--out` for parallel runs. A lock of
a process that doesn't exist anymore is taken over.

While a capture runs, its executable can't be replaced by a build (`LNK1168`, "Device or resource
busy"). Wait for it to finish, or close it if it is stopped in the debugger, then build again.

| File | Content |
| --- | --- |
| `*.png` | Captures, sheets (`*_views.png`, `*_passes.png`, `*_variants.png`, strips) |
| `report.json` | Status, errors, warnings, check failures, expectations, timings, list of outputs |
| `log.txt` | Tool and engine log |
| `scene.json` | Inspection |
| `queries.json` | Query results |
| `trace.json` | Tracked entities |
| `edit_diff.json` | Changes of an edit |
| `profile.json` | GPU timings per pass in milliseconds |
| `validation.json` | Validation messages with `--validation` |

## Workflow for scene changes

1. Inspect and measure first (`--inspect`, `ground`, `entity`, `clearance` queries). Never guess
   coordinates or heights.
2. Change the scene (`edit`) or the script, using measured values. Scripts can query the scene at
   runtime with `scene:CastRay` (see `data/scripts/README.md`).
3. Verify with a job: focus views including `low` views, a strip of every movement, `grounded`
   (with `surfaces`) and `penetration` checks, `path` queries for routes, a walkthrough with player
   input for passability and `resetScripts` for script state.
4. Look at the sheets, not only at the exit code. Overlays make invisible things visible.
5. For rendering changes, render a fixed job before and after the change and `--compose` the
   images, with several sun positions (`settings.sun`).

## Examples

- `examples/sponza_static.json`: inspection, queries, views, passes, grounding check. It reports
  the physics spheres that are placed in the air.
- `examples/sponza_play.json`: playing with a strip, a still and tracking. It reports a sphere
  that sinks into the floor.
- `examples/sponza_lua_raycast.json` + `raycast_test.lua`: verifies the Lua raycast bindings.
- `examples/sponza_lua_components.json` + `component_ref_test.lua`: verifies that components kept by
  scripts stay valid while their pool grows and shrinks.
- `examples/kingdom_gate_guards.json`: closes the Kingdom portcullises with `E` and checks the
  gate movement, the guards (grounded on valid surfaces) and the lanterns.
- `examples/kingdom_walkthrough.json`: the player walks along the road through both gates.
- `examples/kingdom_script_reset.json`: closes the gate, resets the script state and checks that
  the scene stays passable.
- `examples/kingdom_night.json`: switches to night with `N` and checks the lanterns.
- `examples/kingdom_conditions.json`: the walkthrough with conditional events: the player turns and
  operates the gate when reaching positions, a capture follows a log message of the script.
- `examples/kingdom_pause.json`: pauses while the portcullis falls, checks that the gate and the
  player stay still and that the gate closes after resuming.
- `examples/sponza_pipeline.json`: a pipeline of two small edit steps.
- `examples/kingdom_expanded_pipeline.json`: builds `scenes/KingdomExpanded.aescene` from
  `scenes/Kingdom.aescene` in seven steps (base with the dungeon, nature, adventure, landmarks, grass,
  mysteries, mine). The step jobs are generated by the scripts in `examples/kingdom_expanded/`
  (`build_nature.py` and `build_adventure.py` take the approach route `route.json`, the trails come
  from `trails*.json`, shared helpers are in `edit_ops.py`). Regenerate a step instead of editing
  its JSON. The `plan_*.json` jobs survey the original scene, the `play_*.json` jobs play the
  interactions of `kingdom/scripts/kingdomAdventure.lua`, `profile_*.json` measure the meadow and the
  town. The grass step takes about ten minutes.
