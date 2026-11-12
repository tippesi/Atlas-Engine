# AtlasEngineCapture

Renders, plays, measures, checks and edits scenes from JSON jobs with the real GPU (the editor's `MainRenderer`), and
runs the same jobs inside a game with its interface. Every run writes images, `report.json`, `log.txt` and an exit code:
`0` passed, `1` a check or expectation failed, `2` error, `3` crash, `4` the client stopped waiting.

Build: `-DATLAS_CAPTURE=ON` (executable, `ATLAS_CAPTURE_OUTPUT_NAME` names it) or `-DATLAS_CAPTURE_LIBRARY=ON` (only the
library, for game hosts). A running capture tool is renamed aside when it is linked again, a build never fails on it.

## Rules for runs

- Use the client (`run`, `eval`): the server keeps the scene loaded, the world loads once instead of 25 s per run.
- `check` a job before the first run, it finds wrong entity names, Lua that doesn't compile and the preflight errors in
  milliseconds. Every run checks its job before loading anyway.
- Run only what answers the question: one targeted job, the shortest `duration`, `end` once the result is known. A job
  prints how long its previous run took; anything above a few minutes needs a reason, a suite of jobs needs the user.
- Look at `sheet.jpg` (all captures of a job, at most 1536 px wide) instead of single images; let `diff` or `compare`
  decide whether something changed before looking at all.
- Measure with queries and `eval` instead of probe scripts that log values. Never guess coordinates or heights.
- `status` before starting anything on the GPU, `--wait-gpu` to queue behind other engine applications.
- Games: each build passes a short job with `"playerDisplay": true` first, the start a player gets (see Jobs in a game).

## Commands

```powershell
AtlasEngineCapture <job.json> [options]          # direct run in this process
AtlasEngineCapture --scene <scene> [options]     # quick shot or inspection without a job file
AtlasEngineCapture run <job.json | '{inline}'> [job options]   # on the server of this executable, prints the summary
AtlasEngineCapture eval "<lua>" [--play 3] [--scene <scene>]  # the value of Lua in the loaded scene after playing
AtlasEngineCapture wait <command> | status | cancel | stop     # --server <dir> --detach --timeout <s> --wait-gpu
AtlasEngineCapture check <job.json> ...          # mistakes in jobs without loading anything
AtlasEngineCapture inspect <scene|prefab|mesh|model> [--filter <pattern>] [--assembly <pattern>] [--limit n] [--out f]
AtlasEngineCapture inspect <graph.aematgraph> [--filter <pattern>] [--code] [--out f] | inspect nodes [--filter <pattern>]
AtlasEngineCapture diff <a> <b> [--threshold 8] [--out sheet.jpg]   # images or directories, numbers per image
AtlasEngineCapture compose <out.jpg> [--width 1536] [--columns n] [--title t] <image | label=image | dir | pattern> ...
AtlasEngineCapture bench <bench.json>            # A/B timings of builds or shader directories
AtlasEngineCapture kill <pid | captures>         # ends a process with the processes it started
AtlasEngineCapture --serve <dir> | --pipeline <pipeline.json> [--from <step>] [--force]
```

The commands without a scene start in milliseconds, they don't create a window or a GPU device. Job options (also after
the job file, they override it): `--out`, `--name`, `--resolution WxH`, `--render-scale`, `--settle <frames>`,
`--timeout <s>`, `--hidden` (no window, as fast as possible; runs stay visible by default), `--fast` (shown, as fast as
possible), `--no-shots`, `--overlay`, `--play <s>`, `--set path=value` (settings), `--profile <frames>`, `--pause`
(pause at the start of playing), `--hitches [ms]`, `--inspect [file]`, `--sound`, and before the engine starts:
`--validation`, `--streaming`, `--shader-dir <dir>` (relative to `data/`), `--wait-gpu`, `--verbose` (the whole engine log,
otherwise only its errors). Shot options:
`--focus <pattern>`, `--views front,low`, `--camera x,y,z`, `--look-at x,y,z`, `--yaw`, `--pitch`, `--fov`,
`--distance`, `--overview`, `--passes albedo,normal`, `--shot <name>`. Outputs: `bin/capture/<job name>/` or `--out`.

### Client and server

`run`, `eval`, `wait`, `status`, `cancel` and `stop` talk to `bin/capture-server/<executable>/` (`--server <dir>`). The
client starts a server when none runs and replaces one of another build or with other `--streaming`/`--validation`
options once it is idle. Commands queue in file name order, several clients share a server. The server keeps the last
scene loaded; a job for another scene, or after a job that played or edited, loads it again and keeps the meshes,
materials and textures meanwhile. While waiting for commands it presents with vsync, jobs run without. The client waits up to `--timeout` (540 s), then prints the command to `wait` for and
exits with 4; `--detach` returns right away. `cancel` stops the running job, `stop` the server.

The summary: status, duration (load, played), errors and failed checks grouped with counts, failed expectations with
their values, conditions that never happened, `results`, warnings and the outputs. Everything else is in `report.json`
and `log.txt` of the output directory.

Server protocol, for other clients: JSON files in the directory, `{ "command": "run", "job": "<file>" | {inline},
"args": [...] }`, `loadScene` (`scene`, `timeout`, `camera`), `reloadScene`, `unloadScene`, `stop`; results in
`<command>.result.json` (`status`, `exitCode`, `reason`, `report`, `outputDirectory`), processed commands in `done/`.
`server.json` names the process, its build and its state (idle, loading, running), a file named `cancel` stops the job.

### Engine applications on the machine

The capture tool and games linking `AtlasEngineCaptureLibrary` register themselves in `LoadContent` (`InstanceRegistry`,
one file per process in `%LOCALAPPDATA%/AtlasEngine/instances`) with what they do and whether they are busy. The
editor and the demo don't, the engine doesn't know the registry. `status` lists them, runs print the busy ones,
`--wait-gpu` waits until none is busy besides the own server.

## Jobs

```jsonc
{
    "scene": "scenes/Kingdom.aescene",   // relative to data/ or the job; the only required key
    "name": "kingdom", "out": "...",     // defaults: file name, bin/capture/<name>
    "resolution": [1280, 720], "renderScale": 0.75, "settleFrames": 60, "loadTimeout": 300, "fixedDt": 0.016667,
    "showWindow": true, "realTime": true, "pathTrace": false,
    "settings": { "fog": { "density": 0.02 } }, "variants": [ { "name": "noFog", "settings": { "fog": { "enable": false } } } ],
    "inspect": { "file": "scene.json", "filter": ["*Gate*"], "detail": ["Player"] },
    "queries": [], "edit": {}, "shots": [],
    "play": true, "duration": 5.0, "observer": { /* camera */ }, "scripts": ["test.lua", { "script": "x.lua", "entity": "Player" }],
    "timeline": [], "strips": [], "track": { "entities": ["Guard*"], "interval": 0.25 },
    "checks": {}, "compare": { "reference": "reference/", "maxPercent": 0.5, "threshold": 8, "update": false },
    "profile": { "frames": 64 }, "hitches": { "minimumMs": 40, "relativeThreshold": 2 }
}
```

Unknown keys are errors. Entities are name patterns with `*` and `?` (or `#id`); a match of a parent and its children
uses the topmost. A run: check, load (all meshes, terrain and ray tracing data), `settings`, `inspect`, `queries`, `edit`,
`shots` (each after `settleFrames`), `play` with the timeline in steps of `fixedDt`, checks (once, or every
`checks.interval` while playing). The scene file only changes with `edit`.

`capture.project.json` in the job's directory or above: `{ "defaults": { /* job keys, the job replaces them */ },
"preflight": ["python", "tools/lua_check.py"] }`. The preflight runs in its directory before a job that plays, with the
direct run and the client, and fails it with its output.

`settings` patches the feature settings of the scene with merge semantics (`sky` with `clouds` and `atmosphere`,
`postProcessing`, `wind`, `occlusionCulling`, `fog`, `irradianceVolume`, `reflection`, `rtgi`, `ssgi`, `sss`, `ocean`,
an `--inspect` lists them), plus `sun` (`elevation`, `azimuth` 0 = +Z side, or `direction`; `color`, `intensity`,
`shadow: { "cascades": 3, "longRange": true }`) and `materials` (`{ "<path pattern>": { /* .aematerial keys */ } }`,
the running scene only). Patching `ocean` adds one. `postProcessing.upscaler`: 0 none, 1 TAA, 2 FSR2, 3 FSR4, 4 DLSS
(falls back to FSR2). `variants` render all shots again (`settings`, `renderScale`) with a `<shot>_variants.png` sheet.

**Cameras**: `"scene"`, `"overview"`, `{ "position", "lookAt" | "yaw"/"pitch", "groundRelative" }`, focus
`{ "entity", "view", "azimuth", "elevation", "distance", "padding": 1.25, "local", "avoidOcclusion": true }`, follow
`{ "entity", "offset": [0, 2, -6], "local" }`; all `fov`, `near`, `far`. Views: `front` (+Z side), `right`, `back`,
`left`, `top`, `iso`, `low`, `lowSide` (ground level, best to see floating).

**Shots**: `{ "name", "camera", "views": [...], "passes": [...], "settleFrames", "crop": [x, y, w, h] | { "entity", "padding": 0.15 } }`.
Views write `<name>_<view>.png` and `<name>_views.png`, passes `<name>_<pass>.png` and `<name>_passes.png`: `albedo`,
`normal`, `geometryNormal`, `roughness`, `emissive`, `depth`, `velocity`, `stencil`, `materialIdx`, `reflection`,
`volumetric`, `clouds`, `sss`, `gi`. A crop writes `<name>_crop.png` at full resolution. `sheet.jpg` has every capture.

`compare` compares every image with the one of the same path in `reference`: more than `maxPercent` pixels differing by
more than `threshold` fail the job, `update` writes this run as the reference. `report.json` has `comparisons`.

### Playing

The `observer` (default: the scene camera) is the main camera while playing; a focus observer is framed once unless
`dynamic`. Keys move players like the editor's play mode (`W A S D`, `Left Shift`, `Space`), the mouse isn't used for
looking, `look` turns them.

```jsonc
{ "t": 0.0, "teleport": { "entity": "Player", "position": [x, y, z], "yaw": -90 } }
{ "t": 0.0, "look": { "entity": "Player", "yaw": -90, "pitch": 0 } }          // yaw 0 looks along +Z
{ "t": 0.5, "key": "E" }                                                    // or { "key": "W", "action": "down|up|press", "duration": 1 }
{ "t": 0.5, "controller": { "button": "a" } }                               // or { "axis": "lefty", "value": -1 }, held until changed
{ "t": 0.5, "mouse": { "click": [640, 360], "button": "left" } }            // window pixels; "move": [x, y], "down"/"up": "left", "wheel": 1
{ "t": 1.0, "lua": "OdysseyGame.context.daycycle.SetHour(21.5)" }            // or { "file": "probe.lua" }
{ "t": 2.0, "lua": "OdysseyGame.context.quests.stage", "result": "stage" }    // the value goes into results
{ "t": 1.0, "capture": { "name": "now" } }                                 // live; with "camera" or "still": a still
{ "t": 2.0, "expect": { "entity": "Crate", "property": "bottom.y", "op": "<", "value": 0.1 } }
{ "t": 2.0, "expect": { "lua": "#OdysseyGame.context.ships.list", "op": ">=", "value": 3 } }
{ "t": 2.0, "log": "message" }
{ "t": 2.5, "animation": { "entity": "Fox", "animation": "Walk", "model": "skinning/Fox/Fox.gltf", "blendTime": 0.4 } }
{ "t": 3.0, "pause": true }                                               // scripts, physics and players stand still
{ "t": 5.0, "resetScripts": true }                                         // discards non-permanent script environments
{ "when": { "log": "*done*" }, "end": true }                               // ends the job, duration is only the limit
```

Conditions (`when`, `t` is the earliest check, `delay`, `optional`): `{ "log": "text" }` (contains, or a pattern),
`{ "lua": "expression" }` (true when not nil or false) or a property like an expectation. A condition that never
happens fails the job unless `optional`. Expectation properties: `exists`, `count`, `position.x/y/z`, `bottom.y`,
`top.y`, `yaw`, `distanceTo` (`point`), `gapToGround`; operators `< <= > >= == !=` (`tolerance`); every matching entity
has to pass. Lua expectations compare the value with `op`/`value`, without them it has to be true.

**Lua** (`lua` events, conditions, expectations) runs in one environment over the globals of the scene's script state,
so it sees what the game scripts put there (e.g. `OdysseyGame`) and keeps its variables between events. It has `scene`
(also `GetThisScene()`) and `Capture`: `Result(key, value)` (into `results`), `Time()`, `Fail(message)`. Lua errors end
the job. Key names are SDL names (`E`, `Space`, `Left Shift`), controller events attach a virtual game controller
(`a`, `x`, `leftshoulder`, `dpup`, `start`, `leftx`, `lefty`, `rightx`, `righty`, `lefttrigger`; sticks -1 to 1, right
and down positive).

Live captures show the running frame; captures with their own camera are stills on a paused scene. `strips`
(`name`, `from`, `to`, `every`, `columns`, optional `camera`) write `<name>.png` sheets of a movement. `scripts` are
attached for the run (relative to the job or `data/`). `track` writes `trace.json`.

### Checks

```jsonc
"checks": {
    "grounded": { "entities": ["Guard*"], "meshes": ["*/SM_Env_Tree_*.aemesh"], "maxGap": 0.15, "maxSink": 0.3,
                  "surfaces": ["*Ground*"], "ignore": ["*Bush*"], "footprint": 3, "baseHeight": 0.3 },
    "penetration": { "entities": ["*Portcullis*"], "against": ["*"], "margin": 0.05, "minTriangles": 1 },
    "logErrors": true,            // or { "ignore": ["*music.wav*"] }
    "failFast": true, "invalidTransforms": true, "interval": 0.1
}
```

`grounded` compares the bottom of the bounds with the ground below their center (skipping the own hierarchy and
`ignore`), `surfaces` restricts what it may stand on. `footprint` adds a grid of n x n points: where the underside of
the mesh is within `baseHeight` of its bottom, the gap to the ground must not exceed `maxGap` either, e.g. the downhill
edge of a field. `penetration` counts additional intruding triangles of other meshes. `logErrors` fails on engine and
script errors; with `failFast` the job ends at the first one (it would fail anyway) instead of running to its end.

### Queries

Run after loading (`queries.json`) or after an edit (`edit.queries`, `edit_queries.json`). `raycast` and `pick` hit what
is visible, the others what characters collide with (bodies by their shapes, meshes without body, terrain).

```jsonc
{ "type": "ground", "x": -108, "z": -21, "from": 20 }
{ "type": "raycast", "origin": [...], "direction": [...], "ignore": ["Player"], "maxDistance": 100, "bodies": false }
{ "type": "entity", "entity": "Gate*" }                     // position, yaw, bounds, ground below, gap
{ "type": "around", "position": [...], "radius": 5 }
{ "type": "clearance", "entity": "*Portcullis*", "direction": [0, 1, 0], "distance": 10, "samples": 5 }
{ "type": "path", "points": [[x, y, z], ...], "maxStep": 0.3, "radius": 0.4, "spacing": 0.5, "snap": false }
{ "type": "pick", "camera": { ... }, "pixels": [[x, y], ...] }
{ "type": "route", "from": [x, z], "to": [x, z], "via": [], "cell": 2, "margin": 60, "maxSlope": 30, "maxStep": 0.6,
  "clearance": 1, "avoidMaterials": [], "avoid": [], "walkOn": ["*Bridge*"], "ignore": [], "map": "route.png" }
{ "type": "route", "from": [x, z], "to": [x, y, z], "navigation": true }        // on the navigation mesh like the game
{ "type": "surfaceGrid", "min": [x, z], "max": [x, z], "step": 1, "maxHits": 8, "tolerance": 0.25, "ignore": [] }
{ "type": "corridor", "points": [[x, y, z], ...], "width": 3, "height": 3, "ignore": ["Grass*"] }
{ "type": "collisionCoverage", "center": [x, z], "radius": 50, "minSize": 0.5, "meshes": [], "ignore": ["*Grass*"] }
{ "type": "terrain" }  { "type": "terrainSample", "points": [[x, z]] }  { "type": "buried", "entity": "Dungeon", "margin": 1 }
{ "type": "terrainMap", "file": "map.png", "min": [x, z], "max": [x, z], "size": 2048, "grid": 100, "contour": 5,
  "highlight": [], "markers": [{ "position": [x, z], "radius": 10, "label": "hole" }], "paths": [] }
{ "type": "materialGraphShaders", "directory": "x/graphs", "library": true, "spirvDirectory": "out" }
```

- `surfaceGrid`: vertical rays on a grid (`min`/`max`, `center`/`radius` or the bounds of `entity`) with every surface
  they pass; groups them into levels (height, up or down facing, coverage, the surfaces) and counts points where the
  terrain lies above a floor (it stands in a room). The grid goes to `surface_grid.json`. Floors, ceilings, decks.
- `corridor`: meshes reaching into a corridor along a path (`along`, `lateral` distance, bounds), e.g. a road.
- `collisionCoverage`: visible meshes larger than `minSize` without a body on them or a parent, grouped by mesh.
- `clearance` samples rays from a face of the bounds; `path` checks steps and obstacles at knee and chest height;
  `pick` names the entity and material at pixels; `route` is A* over a grid around the stops (holes, slopes, avoided
  materials, objects; `map` shows why cells are blocked) or Detour with `navigation`; `terrainSample` gives height,
  slope, material and holes; `terrainMap` draws a top down map; `buried` lists meshes above the terrain or below holes;
  terrain queries keep all cells loaded.

### Editing

`edit`: `operations`, `output` (default `<scene>.edit.aescene`), `overwrite`, `saveUnchanged`, `dryRun`, `queries`,
`textureFormat` (`ktx2` for imported models). The edit writes `edit_diff.json`, runs the checks and only saves when they
pass, then loads the saved scene again to verify it. Materials and graphs an edit changed without saving them (dry
runs, failed checks) go back to their files before the server runs the next job. An overwritten scene is backed up
(`.backup-<time>`). `edit` and `play` don't combine, verify an edit with a second job. Selections need one topmost
match unless `"all": true`; `position` is world space, `localPosition`/`rotation`/`scale` parent space.

```jsonc
{ "op": "snapToGround", "entity": "Barrel", "offset": 0 }   { "op": "translate", "entity": "#4711", "delta": [0, -6, 0], "space": "world" }
{ "op": "setTransform", "entity": "Gate", "position": [x, y, z], "rotation": [0, 90, 0], "scale": [1, 1, 1] }
{ "op": "rotate", "entity": "Statue", "yaw": 45 }   { "op": "rename", "entity": "Entity 7", "name": "Crate" }
{ "op": "setVisible", "entity": "Debug*", "visible": false, "all": true }   { "op": "delete", "entity": "Old*", "all": true }
{ "op": "duplicate", "entity": "Lantern", "name": "Lantern 2", "position": [x, y, z] }
{ "op": "add", "name": "Ramp", "mesh": "m.gltf", "body": true, "position": [...], "parent": "Level", "localPosition": [...],
  "visible": true, "light": { "type": "point|spot", "color", "intensity", "radius", "direction", "innerAngle", "outerAngle", "shadow" },
  "fogVolume": { ... }, "script": "x.lua", "animation": "Run", "animationSpeed": 1, "parentJoint": 3 }
{ "op": "prefab", "prefab": "prefabs/X.aeprefab", "name": "X", "parent": "Level", "position": [...], "rotation": [...] }
{ "op": "makePrefab", "scene": "scenes/Demo.aescene", "output": "prefabs/X.aeprefab", "entity": "Name", "origin": [...],
  "area": { "center": [x, y, z], "radius": r }, "exclude": [], "scripts": false, "withoutBodies": ["*Grass*"] }
{ "op": "replaceMesh", "meshes": ["*Tree_01*.aemesh"], "with": ["Tree_03.gltf"], "matchHeight": true, "seed": 0, "within": {...} }
{ "op": "replaceWithPrefab", "meshes": "*Brazier*.aemesh", "exclude": [], "removeNear": [], "prefab": "...", "name": "Fire Bowl" }
{ "op": "scatter", ... }   { "op": "clearArea", "entities": ["Grass*"], "area": { "center": [x, z], "radius": 20 } }
{ "op": "meshBodies", "meshes": [...], "replace": true, "remove": false, "motion": "dynamic", "shape": "box|convex|boxes",
  "density": 500, "buoyancy": 1, "inOcean": false, "supported": false }
{ "op": "constraint", "meshes": [...], "type": "point|hinge|cone|distance|slider|fixed", "anchor": "top", "connect": "...",
  "axis": [...], "limits": [0, 4], "worldPoint": [...], "worldOffset": [...], "breakForce": 0 }
{ "op": "trigger", "name": "Zone", "shape": "sphere|box|cylinder", "radius": 3, "halfSize": [...], "position": [...] }
{ "op": "material", "material": "x.aematerial", "set": { "metalness": 0, "roughnessMapPath": null } }
{ "op": "materialCopy", "material": "a", "output": "b", "set": {}, "remove": [] }   { "op": "meshMaterials", "meshes": [...], "replace": { "a": "b" } }
{ "op": "meshCopy", "mesh": "a.aemesh", "output": "b.aemesh", "invertNormals": true, "set": {}, "assign": ["Grass*"] }
{ "op": "meshSettings", "meshes": "x/*.aemesh", "set": { "cullBackFaces": false, "movable": true, "animations": "..." },
  "impostor": { "views": 12, "resolution": 256, "distance": 150, "textureFormat": "ktx2" } }
{ "op": "compressTextures", "directories": ["ancient"] }   { "op": "bakeMaterial", "material": "...", "mode": "textures|average" }
{ "op": "materialGraph", "graph": "x/graphs/fire.aematgraph", "create": true, "from": "y.aematgraph", "name": "Fire", "maxDisplacement": 0.3,
  "nodes": [ { "key": "noise", "type": "Fractal noise", "inputs": { "Position": "Object position", "Scale": 5.5 } },
             { "key": "glow", "type": "Float parameter", "name": "Glow", "value": 8, "range": [0, 30], "group": "Light" },
             { "node": "Surface output", "inputs": { "Emissive": "glow", "Roughness": 0.8, "Opacity": null } },
             { "node": "#12", "remove": true } ] }
{ "op": "assignMaterialGraph", "materials": "x/materials/M_Fire_*.aematerial", "graph": "x/graphs/fire.aematgraph",
  "values": { "Glow": 8, "Tint": [1, 0.4, 0.4] }, "textures": { "Albedo": "x/t.png" }, "reset": ["Bend"] }
{ "op": "terrainCopy", "path": "terrains/x/x.aeterrain" }   { "op": "shoreLine", "oceanHeight": 0, "resolution": 2048 }
{ "op": "terrainCreate", "path": "...", "heightMap": "h.png", "splatMap": "s.png", "materials": [...], "LoDCount": 6,
  "patchSize": 16, "resolution": 1, "height": 300, "translation": [...] }
{ "op": "terrain", "brushes": [ { "type": "flatten|raise|smooth|hole|fill|material|path", "position": [x, z], "radius": 12, ... } ] }
{ "op": "detachTerrain" }   // dry runs only, to look at what lies below
```

- `materialGraph` creates a graph file (`create`, `from` copies one) or changes one. `nodes` entries add a node (`type`,
  `key` to refer to it) or change one (`node`: `#id`, a key, a parameter name or a node type which occurs once;
  `remove`). Fields: `name`, `value`, `range`, `group`, `order`, `size`, `texture` (texture parameters), `position`
  (default: the column of its way to the outputs, the editor's Arrange tidies up) and `inputs`: a link (`"key"`, or
  `"key.Pin"` for nodes with several outputs), a number or array as the value of the input, `null` disconnects. The
  graph has to compile. Loaded materials show the change right away, the file is saved with the materials (`.backup`
  once). `inspect nodes` lists the node types with their pins, `inspect <graph>` the nodes with their
  links and stages.
- `assignMaterialGraph` sets the graph of materials (`null` removes it, without `graph` they keep theirs) and their
  parameter `values` and `textures`; values of parameters the graph doesn't have are dropped, `reset` (`true` or names)
  goes back to the defaults. Numbers set all components, arrays the first ones; colors are sRGB. Saved like `material`.
- `check` applies `materialGraph` and `assignMaterialGraph` without loading anything: unknown types, pins, parameters
  and compile errors with the node.
- `add` imports model files next to them (`meshes/`, `materials/`); `body` adds a static mesh shape. Spot lights with
  `shadow` stay dark this way.
- `meshBodies`/`constraint`/`trigger`: bodies follow their world scale, `remove` drops them (grass the player walks
  through), dynamic bodies float with `"ocean": { "physics": true }`.
- `replaceMesh`/`replaceWithPrefab` keep the bottom of the replaced mesh (pivots differ between packs); bodies of
  replaced meshes are removed.
- `meshSettings`, `compressTextures`, `materialCopy`, `meshCopy`, `meshMaterials`, `makePrefab`, `terrainCopy` write
  their files even in a dry run; shared assets keep a `.backup` once.
- `terrain` brushes only work on a copy (`terrainCopy`) or with `"overwriteTerrain": true`; they don't move what stands
  on the ground: `clearArea`, brush, `scatter` again. `flatten` blends over `falloff` meters.
- `scatter`: instances inside `area` (`center`/`radius`/`innerRadius`, `min`/`max`, `polygon`, `path`/`width`) outside
  `exclude`, for a `seed`: `count`, `attempts`, terrain `materials`, `slope`, `height`, `noise` (`scale`, `threshold`,
  `softness`), `spacing`/`spacingWith`, `clearance`, `ignore`, `maxBlockerSize`, `onMeshes`, `castFrom`, `append`,
  `required`, `items` (`mesh`, `weight`, `scale`, `scaleY`, `flatness`, `yaw`, `tilt`, `pitch`, `align`, `sink`,
  `pivotOnGround`, `radius`, `body`: `mesh|capsule`, `name`, `light`). The lowest point of the bounds goes on the ground.

## Pipelines

`--pipeline <file>` runs edit jobs in one process, each on the result of the step before, and keeps a snapshot after
each step (saved scenes, changed materials and the pipeline's `files`). A run starts at the first step whose key (job,
`inputs`, the keys before) changed, from the snapshot before it. `"independent": true` steps (imports, impostors) are
skipped while their job and inputs are unchanged. `--from <step>`, `--force`, `--no-shots` (faster builds).

```jsonc
{ "name": "world", "steps": ["step_1.json", { "name": "move", "job": "step_2.json", "inputs": ["x.lua"], "independent": false }],
  "files": ["prefabs/x.aeprefab"], "snapshots": "bin/capture/world/snapshots" }
```

## Jobs in a game

A game host linking `AtlasEngineCaptureLibrary` runs a job with `--capture <job.json>` (`HostedJob.h`): the game loads
and renders its own scene with its interface, the job plays its timeline once the game runs, with the job's fixed time
step, keys, controller, mouse, Lua in the game's script state, expectations, conditions, checks, `track`, queries and
`settings`. Captures are pictures of the window with the interface, as live captures and strips; `sheet.jpg` and the
summary as usual. Shots, edits, variants, observers and capture cameras need the capture tool. The window has to stay
visible; hosts keep it windowed and present without vsync during a job, with vsync a display that is turned off holds
every frame back (the Sandbox ran at 4 fps). Odyssey: `OdysseyOfAtlas.exe --capture capture/x.json [--dev-scripts]`.

That window is never what a player gets. Each build of a game first has to pass a job with `"playerDisplay": true`: the
host starts with the player's display settings (fullscreen, vsync), like a normal start. Until then the other jobs refuse
to run (`.cache/capture-player-display-<executable>.txt` next to the executable records the build). Keep that job short,
e.g. the start of the game up to the first seconds of playing.

## Measuring

- `profile` averages the GPU passes and the CPU profiler over the last frames (`profile.json`, at most 64 frames in a
  job, `--profile` for more), `hitches` records slow frames with their CPU profile (`hitches.json`), `--validation`
  writes the Vulkan validation messages by id (`validation.json`), `report.json` lists the GPU memory (`gpuMemory`).
- `bench <bench.json>` follows the performance rules of `AGENTS.md`: builds (`"builds": { "base": "x_base.exe", "change":
  "x.exe" }`) or shader directories (`"shaders": { "base": "<dir>", "change": "shader" }`, the differing files are touched
  before each run so the SPIR-V cache doesn't serve the other one) run interleaved in alternating order at 1920x1080,
  `frames` (240) rendered and the last 64 profiled, auto exposure off and the scene paused; `rounds` (2) and up to `maxRounds` (5) while the frame
  total is within twice its standard error and above 1 %. It prints every pass (or `passes`) as mean (min - max) per
  build with the verdict faster, slower or inconclusive, and the pixel difference of the builds next to that of two
  runs of one build. Results in `bin/capture/bench/<name>/bench.json`.

## Output files

`report.json` (status, reason, errors, warnings, checks, expectations, results, comparisons, conditional events,
timings, outputs, GPU memory), `log.txt`, `sheet.jpg`, captures and sheets (`*_views.png`, `*_passes.png`,
`*_variants.png`, strips, `*_crop.png`), `scene.json`, `queries.json`, `edit_queries.json`, `edit_diff.json`,
`trace.json`, `profile.json`, `hitches.json`, `validation.json`. A run removes the outputs of the previous run listed in
its `report.json` and marks the directory with `capture.lock` (a second run on it fails with the owner). Every run has a
`runId`.

## Source layout

- `App`: the executable: direct runs, `--serve`, `--pipeline` and the phases of a job (load, prepare, edit, shots, play,
  finish) with its renderer (`CaptureRenderer`, `Overlay`).
- `Player`, `Timeline`, `LuaJob`: the timeline of a playing job, its input, conditions and Lua, shared with `HostedJob`.
- `Job`, `Check`: job files, the command line and the project file; checks before loading.
- `Report`, `Summary`, `Checks`: the outputs, the printed summary, checks and expectations.
- `Tools`, `Client`, `Server`, `Process`, `InstanceRegistry`: commands without the GPU, the client and server, processes,
  the running engine applications.
- `SceneEditor`, `SceneTools`, `TerrainTools`, `Scatter`: edit operations, queries and brushes, each a function in a
  table by its name. `SceneFile`, `Inspect`: reading scene, prefab and mesh files without loading them.
- `Pipeline`, `Bench`, `Image`: pipelines, benchmarks, images, sheets and differences.

Engine notes that used to be here are in `wiki/EngineNotes.md`.
