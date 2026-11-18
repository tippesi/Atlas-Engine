# Feature index

What the engine, the editor and the capture tool already do, one line per feature with the words someone would search
for. Read it before writing a helper, tool, script or workaround, and before writing that something is missing. Paths
are relative to `src/engine/` unless they start with another folder, Lua names are the bound names in `Atlas` (types,
enums, tables) and `Glm`. Add or change a line in the same change as the feature: `python wiki/check_features.py`
fails when a component, Lua type, capture event, operation, query or check, engine folder, helper header or editor
tool isn't listed here, or a listed file doesn't exist.

## Scene and entities

- **Scene** (level, world, map): `scene/Scene` owns the entities (`ecs/`), resources, physics world, terrain, ocean,
  sky, wind and the feature settings. `Update`, `Merge` (append another scene), `Clear`, `CreatePrefab`,
  `WaitForResourceLoad`/`IsFullyLoaded` (all meshes and textures loaded), `paused` (scripts, physics, wind and the
  scene time stand still). Saved as `.aescene` by `scene/SceneSerializer`. Lua `Scene`.
- **Entities** (objects, game objects, nodes): `scene/Entity`, `Scene::CreateEntity`, `DestroyEntity`,
  `DuplicateEntity`, `GetEntityByName`, `MarkEntityDirty` (GPU instances only refresh for dirty entities),
  `scene/Subset` and `scene/SceneIterator` to loop over entities with given components. Lua `Entity`.
- **Hierarchy** (parent, children, attach, group): `HierarchyComponent`, `Scene::GetParentEntity`,
  `GetHierarchyBounds`; children follow the transform of their parent.
- **Transforms** (position, rotation, scale, matrix, placement): `TransformComponent` (local and global matrix,
  static or movable), `MatrixDecomposition` (matrix to position, rotation, scale), `TransformOrientation` (mirrored
  transforms). Lua `TransformComponent`, `MatrixDecomposition`.
- **Names and tags**: `NameComponent`; `ObjectComponent` is an empty marker component.
- **Prefabs** (templates, reusable groups, blueprints): `.aeprefab` files, `Scene::CreatePrefab`; C++ entity helpers in
  `scene/prefabs/` (`Node`, `MeshInstance`, `Group`, `AudioVolume`). Capture `makePrefab`, `prefab`,
  `replaceWithPrefab`.
- **Script state** (Lua globals, evaluate Lua next to the scripts): `Scene::GetLuaState`, the scripts of a scene share
  it.
- **Scripts** (gameplay code, behaviour, Lua): `LuaScriptComponent` runs a Lua file per entity with typed properties
  (`ScriptPropertyType`), permanent scripts keep their state when play mode ends. `scripting/LuaScriptManager`,
  bindings in `scripting/bindings/`, see *Lua* below.
- **Time** (clock, delta time, pause): `Scene::GetTime`/`GetDeltaTime` for everything that animates, `Clock` (Lua
  `Clock`) only for real time UI and profiling.
- **Space partitioning** (spatial index, nearby objects, area query): `scene/SpacePartitioning`, `Octree`;
  `Scene::QueryAABB` (Lua `scene:QueryAABB(aabb, components, nameParts)`) finds entities in a box.

## Geometry queries and volumes

- **Ray casts** (raycast, line of sight, hit test, ground height, pick): `Scene::CastRay` (Lua `scene:CastRay`,
  `RayHit`, filters `SceneQueryComponents`) hits physics bodies by their shapes and meshes without a body by their
  triangles. `Scene::CastShape`, `CollideShape` (sweeps, overlaps). `Ray` also builds pick rays from a viewport pixel.
- **Bounding volumes**: `volume/` (`AABB`, `Frustum`, `Rectangle`, `BVH`, `Octree`, `Ray`), Lua `AABB`, `Ray`,
  `Frustum`. `BoundsChangeLog` records changed areas that several consumers read (navigation rebuilds).
- **Terrain height** (ground, elevation, slope): `TerrainTool::GetHeight`, ray casts against the terrain, capture
  `ground` and `terrainSample`.
- **Water height** (waves, buoyancy, swimming, boats): `OceanSurfaceQueries` (Lua `OceanSurfaceQueries`,
  `OceanSurface`) samples the simulated ocean surface on the CPU.

## Physics

- **Rigid bodies** (collision, colliders, dynamics, kinematic, static): `RigidBodyComponent`, created from
  `RigidBodyCreationSettings`; `physics/PhysicsWorld` (Jolt), `physics/Body`, `MotionQuality`, collision `Layers`
  (persisted, append new ones; `Cloth` for soft bodies: static, kinematic and `ClothCollider` bodies only, not the player; `ClothCollider` kinematic capsules on bones that only cloth touches; scene queries pass both). Kinematic bodies follow their transform.
- **Shapes** (mesh collider, convex hull, box, capsule, height field): `Shape`, `ShapesManager`, `MeshShapeSettings`,
  `ConvexHullShapeSettings`, `BoundingBoxShapeSettings`, `SphereShapeSettings`, `CapsuleShapeSettings`,
  `CylinderShapeSettings`, `HeightFieldShapeSettings`, `CompoundShapeSettings`, `CompoundShapeChild`, `ShapeType`.
- **Soft bodies** (cloth, flags, curtains, sails, deformable bodies, balloons, Jolt soft body): `SoftBodyComponent` with
  `Physics::SoftBodySettings` (compliance, `SoftBodyBendType`, pressure, object layer `Cloth`, pin bounds in mesh space,
  bounds without a vertex pin the closest one). The `Mesh::SoftBodySimulationMesh` is created once per mesh at runtime:
  level 0 welded by position, doubled back faces dropped, simplified to `SoftBodyMeshSettings::maxSimulatedVertices`
  (proxy/cage mesh, regularized even triangles, optionally `keepBorders`), every render vertex bound to a simulated
  triangle, vertex masses by area. It and the Jolt settings are prepared in jobs; bodies with the same geometry and
  constraint settings share the Jolt settings (`ShapesManager`, rotation goes to the body, scale into the vertices). Only
  entities with a simulated body are drawn from their vertices (`SOFT_BODY` pipeline variants, own GPU scene slot,
  `raytracer/deform.csh`), the other instances of the mesh keep levels of detail and impostors. The mesh editor sets the
  resolution and simulates a preview; the soft body component panel edits the pins with a box gizmo and shows the
  simulation mesh in the scene and the mesh editor. Lua `AddSoftBodyComponent(SoftBodySettings())`, mesh
  `SoftBodyMeshSettings`. `windFactor` drags the free vertices towards the scene wind with a flutter,
  `simulationDistance` simulates only near the main camera (farther the mesh rests in its rest pose). Capture edit op
  `softBodies` (meshes, set, pin `top`/`topBottom` per connected part, small parts fixed, `set` for the pinBounds of
  "set"), `meshSettings` `softBody`.
- **Constraints** (joints, ropes, hinges, chains, anchors): `ConstraintComponent`, `ConstraintCreationSettings`,
  `ConstraintType`. Capture `constraint`.
- **Triggers and contacts** (sensors, overlap events, collision callbacks): sensor bodies, `Scene::GetContactEvents`,
  `ContactEvent`, `ContactEventType`. Capture `trigger`.
- **Character controller** (player, walking, jumping, capsule character): `PlayerComponent` (`physics/Player`),
  moved by `input/PlayerInput` (keyboard, mouse and controller).
- **Buoyancy** (floating bodies, fluid drag, boats): bodies in the ocean float when the ocean has physics; capture
  `meshBodies` with `buoyancy`.

## Navigation

- **Navigation mesh** (pathfinding, AI movement, Recast, Detour, walkable area): `navigation/NavigationMesh` builds
  tiles around a center from static bodies and the terrain and rebuilds them where bodies or heights change.
  `scene->navigation` (`navigation/Navigation`, `NavigationSettings`) keeps one around the main camera. Lua
  `Navigation`, `NavigationMesh`, `NavigationSettings`. Editor `NavigationPanel`, capture `route` with navigation.

## Terrain

- **Terrain** (heightmap, landscape, ground, LOD, holes, caves): `terrain/Terrain` with `TerrainStorage` cells loaded
  around the camera, materials from a splat map, holes (height 65535). `.aeterrain` files.
- **Terrain editing** (sculpt, brush, flatten, smooth, raise, paint, holes): `tools/TerrainTool` (`BrushHeight`,
  `SmoothHeight`, `FlattenHeight`, `BrushMaterial`, `BrushHole`, `FillHoles`, `GenerateTerrain` from a height map).
  Editor `TerrainPanel`, capture `terrain`, `terrainCreate`, `terrainCopy`, `detachTerrain` (dry runs only).
- **Terrain generation** (procedural landscape, biomes, noise terrain): editor `TerrainGenerator` (elevation biomes),
  `NoiseGenerator`.
- **Shore** (coast, beach waves): `Terrain::shoreLine`, capture `shoreLine` bakes it for the ocean shore waves, `calm` areas keep harbor basins free of them.

## Vegetation, scatter and clutter

- **Vegetation generation** (forest, foliage, biomes, plant growth, procedural placement): editor
  `VegetationGenerator` (growth based, `VegetationGeneratorPanel`) on top of the `TerrainGenerator` biomes.
- **Scatter** (place many meshes, distribute, populate, decorate): capture `scatter` (area or polygon, slope, height,
  material and exclusion filters, seeds), `clearArea` removes instances again. Editor `GeometryBrushWindow` places
  entities with a brush.
- **Clutter and grass** (instanced foliage, ground cover): `scene/Clutter`, `ClutterComponent` (an instance as a
  4x3 matrix), `actor/` (`VegetationActor`), drawn by the vegetation renderer (`VegetationHelper`).
- **Wind** (sway, bending, cloud movement): `scene/Wind`, Lua `Wind` (`scene.wind`), ImGui `WindPanel`; vegetation and
  clouds follow it.

## Ocean and water

- **Ocean** (sea, water, waves, wave simulation, underwater): `ocean/Ocean`, `OceanSimulation`, `OceanNode`, saved
  with the scene (`ocean/OceanSerializer`). Lua `Ocean`, `OceanSurface`, `OceanSurfaceQueries`, ImGui `OceanPanel`.

## Lighting, sky and atmosphere

- **Lights** (sun, point, spot, lamp, torch, directional): `LightComponent` with `LightType`, `LightMobility`,
  `TypeProperties`, `DirectionalLightProperties`, `PointLightProperties`, `SpotLightProperties`; `Scene::GetMainLight`.
- **Shadows** (shadow maps, cascades, cube shadows): `Shadow`, `ShadowView` (Lua), terrain and cloud shadows in their
  renderers.
- **Sky** (day, night, time of day, sun position, skybox, HDR, sky probe): `lighting/Sky`, `Atmosphere`, `Skybox`,
  `EnvironmentProbe`, Lua `Sky` (`scene.sky`), `Atmosphere`, `EnvironmentProbe`, `Cubemap`, ImGui `SkyPanel`.
- **Clouds** (volumetric clouds, cloud shadows, overcast): `VolumetricClouds` (authoring parameters only),
  `VolumetricCloudsScattering`, ImGui `VolumetricCloudsPanel`; moved by the wind. There is no weather system (rain,
  snow) in the engine.
- **Fog** (haze, mist, volumetric fog, god rays, fog volumes): `lighting/Fog` (Lua `Fog`, `scene.fog`, ImGui
  `FogPanel`), local volumes `FogVolumeComponent` (`FogVolumeShape` box or ellipsoid), froxel volumetrics.
- **Global illumination** (GI, bounce light, indirect light, probes): `IrradianceVolume` (DDGI probes, ImGui
  `IrradianceVolumePanel`), `RTGI` (ray traced, `RTGIPanel`), `SSGI` (screen space, `SSGIPanel`), `AO`.
- **Reflections**: `Reflection` (ray traced, `ReflectionPanel`), environment probes.
- **Subsurface scattering** (skin, leaves, translucency): `SSS` (`SSSPanel`), material transmission.

## Cameras and post processing

- **Cameras** (view, FOV, main camera): `CameraComponent`, `Scene::GetMainCamera`.
- **Moving along curves** (spline, curve, camera path, rail, track, flight, dolly, keyframes): `SplineComponent` with
  `SplineControlPoint` (transform and time), `SplineType` (linear, Catmull-Rom, Hermite), `SplineBakeMode` (equal or
  distance spacing for a constant speed), `GetInterpolated` returns a `SplinePoint` (position, rotation, tangent). Lua
  `SplineComponent`, editor gizmos for the control points. Nothing moves an entity along a spline on its own yet, a
  script reads `GetInterpolated` every frame.
- **Post processing** (exposure, bloom, vignette, film grain, chromatic aberration, sharpening, tone mapping):
  `postprocessing/` (`PostProcessing`, `AutoExposure`, `Bloom`, `Vignette`, `FilmGrain`, `ChromaticAberration`,
  `Sharpen`), Lua types of the same names, ImGui `PostProcessingPanel`.
- **Anti-aliasing and upscaling** (TAA, FSR, DLSS, render scale): `TAA`, `Upscaler` (`PostProcessing::upscaler`),
  `renderer/UpscalerAddons` with the `libs/upscalers` add-ons and an FSR2 fallback, jitter from `HaltonSequence`.

## Rendering

- **Renderer** (frame, passes, G-buffer, deferred): `renderer/MainRenderer` coordinates the renderers in `renderer/`,
  render targets in `renderer/target/`, pipelines from `pipeline/` (`PipelineManager` caches variants), Vulkan in
  `graphics/`, GPU buffers in `buffer/`, textures in `texture/`.
- **Debug drawing** (lines, gizmos, shapes, wireframe): `renderer/PrimitiveBatch` (lines and shapes), editor
  `PrimitiveBatchWrapper`, `Guizmo`; capture overlays.
- **Text** (3D text, labels, signs, fonts): `TextComponent` with a `Font` (Lua), `renderer/TextRenderer`.
- **Decals** (projected textures, animated sprites): `Decal`, `renderer/DecalRenderer`.
- **Culling** (frustum culling, occlusion culling, HiZ, GPU driven, render lists): `RenderList`, `scene/GPUScene`,
  `culling/` shaders, `Scene::occlusionCulling`.
- **Geometry helpers** (cube, sphere, grid, quad): `GeometryHelper`; `CommonStructures` (GPU structs shared by
  renderers), `RayTracingHelper` (shared ray tracing passes).
- **Path tracing** (reference render, ground truth): `renderer/PathTracingRenderer`.

## Ray tracing

- **Acceleration structures** (TLAS, BLAS, hardware ray tracing): `raytracing/RayTracingWorld`, `RayTracingManager`,
  `BLAS`/`TLAS` in `graphics/`; animated meshes get skinned BLASes.
- **Deformed ray tracing geometry** (skinning, wind, material graph vertex offsets in ray tracing, position fetch):
  `RayTracingWorld` deforms nearby instances with `raytracer/deform.csh` (`skinnedBlasDistance`, `deformedBlasDistance`),
  hits read positions from the BLAS and normals of deformed instances in `raytracer/surface.hsh` (`GetTriangle`).
- **Ray tracing pipelines** (raygen, hit objects, shader execution reordering, SER): `RayTracingPipelineDesc`,
  `CommandList::TraceRays`/`TraceRaysIndirect`, `RayTracingHelper::GetRayTracingPipelineConfig`; RTGI, DDGI,
  reflections and the path tracer use them on devices with hit objects (`VK_EXT_ray_tracing_invocation_reorder`, `raytracer/bvh.hsh`), the
  others trace with ray queries; `shaderExecutionReordering` per effect.
- **Material graphs in ray tracing** (graph surfaces in reflections, RTGI, DDGI and the path tracer, hit groups, closest
  hit shaders): the graph compiler writes a `_hit.rchit` per graph (`CompiledGraph::hitFile`), instances select the hit
  groups per geometry (`RayTracingWorld::GetShaderBindingTable`) and `GetSurfaceParameters` executes them, graph opacity
  cuts out hits in a `_opacity.rahit` any hit stage (`raytracer/graphOpacity.rahit`, with `opacityCheck`), graphs use their
  baked values until their pipeline is compiled in the background (`RayTracingHelper::GetRayTracingPipeline`,
  `RayTracingWorld::GetMaterialGraphPipelineConfig`); compute paths use the bake. The path tracer can turn graphs off for
  debugging (`PathTracingRenderer::materialGraphs`).

## Meshes, materials and impostors

- **Meshes** (models, geometry, LOD, mobility, culling distance, screen size): `mesh/Mesh`, `MeshData`, `MeshComponent`
  (Lua `Mesh`, `MeshData`, `MeshComponent`, `MeshMobility`, `MeshUsageBits`), `.aemesh` files, editor
  `MeshEditorWindow`.
- **Mesh simplification** (decimation, meshoptimizer, weld, remap, vertex cache): `mesh/MeshSimplifier` with
  `SimplifyOptions` (target ratio or count, max error, locked borders, regularization for even triangles, attributes, per vertex locks), welding by position
  and the remap/optimization steps around it. The engine uses it instead of meshoptimizer directly (levels of detail,
  soft body simulation meshes, capture `softBodies`).
- **Mesh levels of detail** (LOD, simplification, decimation, meshoptimizer, detail levels): `MeshData::GenerateLods` with
  `MeshLodSettings` (`lodIndices` follow level 0 in the index buffer, each level has fetch-ordered copies of its
  vertices behind the ones of level 0), generated on import (`ModelImporter::lodSettings`) and in a job by
  `Mesh::GenerateLods`/`ClearLods` (editor mesh panel, Lua, capture `meshSettings` `lods`), which the scene update takes
  over (`Mesh::ApplyGeneratedLods`). The GPU culling picks a level per instance by `Mesh::lodPixelError` times
  `Scene::meshLodErrorScale` (one multi draw per submesh), host culled passes with `Mesh::SelectLod`
  (`RenderList::GetLod`); ray tracing traces the same level (`Mesh::lodBlases`), meshes whose LOD BLASes got no bindless
  slot are drawn only with the levels the ray tracing has. Meshes store their `MeshData::lodSettings`.
  `Scene::debugMeshLods` colors the levels, `Scene::forcedMeshLod` shows one level everywhere (viewport settings of
  the scene and mesh editors), capture `inspect` lists their triangles.
- **Skeletons** (bones, rig, joints): `Skeleton`, `Joint` (Lua).
- **Materials** (PBR, textures, emission, transmission): `Material` (Lua), `.aematerial` files, editor
  `MaterialEditorWindow`, ImGui `MaterialPanel`, `MaterialsPanel`. Capture `material`, `materialCopy`, `meshMaterials`.
- **Material graphs** (node materials, shader graph, procedural textures, vertex animation, displacement):
  `materialgraph/` (`.aematgraph`, `Compiler`, `NodeLibrary`, `Baker`), editor `MaterialGraphPanel`, see
  `wiki/MaterialGraphs.md`. Capture `materialGraph` (create and change graphs: nodes, links, parameters),
  `assignMaterialGraph` (graph and parameter values of materials), `bakeMaterial`, query `materialGraphShaders`,
  `inspect` of graphs and `inspect nodes` (node library with pins). Object/world space in the vertex stage:
  `Object to world position`/`World to object position` (instance rotation and scale) and the `direction` nodes.
  `Texture coordinates 2` reads the second UV set of a mesh in the vertex stage (`MeshData::texCoords1`, imported
  from the second UV channel, e.g. Pivot Painter data).
- **Code node in material graphs** (custom GLSL, custom expression, shader snippet): node type `Code` with the body of
  a GLSL function, own inputs and result type (`Node::code`, `NodeLibrary::UpdateCodeDefinition`), edited in the
  inspector of `MaterialGraphPanel`, capture `materialGraph` node keys `code`, `codeInputs` (`[name, type]` pairs),
  `codeOutputType` (types `float`, `vec2`, `vec3`, `vec4`).
- **Impostors** (billboards, far LOD): `mesh/Impostor`, baked by `tools/ImpostorTool` with
  `renderer/ImpostorRenderer` (material graph surfaces through `materialgraph/Evaluator`), loaded by
  `loader/ImpostorLoader`, streamed by `streaming/`.

## Animation

- **Skeletal animation** (clips, blending, layers, IK, animation LOD): `AnimationComponent` (Lua) with
  `AnimationLayer` (override or additive, masks), `AnimationIKChain` (two bone IK, foot locking),
  `AnimationBlendMode`; `animation/` (`Animation` with `MeasureStride` for the ground a cycle covers,
  `AnimationChannel`, `.aeanimation` files). Editor `AnimationTimelinePanel`, capture `animation` event.
- **Animation transitions** (inertialization, dead blending, smooth clip changes, crossfade from the shown pose):
  `AnimationComponent::Transition` (Lua `Transition`) fades the pose shown after all layers into whatever plays next,
  the faded pose keeps its motion and slows down (`AnimationPoseFade`); blends of `Play` work alike and ease in and out.

## Audio

- **Sound** (music, effects, 3D audio, positional, reverb, low pass, ambience zones): `audio/` (`AudioManager` with
  `AudioReverbSettings`, `AudioData`, `AudioStream`), `AudioComponent` (an emitter), `AudioVolumeComponent` (area
  sound with falloff), `Scene::PlayAudio` (positional one shot). Lua types of the same names.

## Input and events

- **Input** (keyboard, mouse, gamepad, controller, key bindings): `input/` (`Keyboard`, `Mouse`, `Controller`,
  `KeyboardMap`, `ControllerMap`, `PlayerInput`), Lua `Keycode`, `KeyboardMap`, `ControllerMap`, `ControllerButton`,
  `ControllerAxis`.
- **Events** (callbacks, window, drop, text input): `events/EventManager` with `EventDelegate`s.

## Resources, loading and streaming

- **Resources** (asset manager, handles, async loading, cache): `resource/ResourceManager`, `ResourceHandle`, Lua
  `ResourceManager` (with `GetOrLoad*Async`). Missing resources don't block loading.
- **Loading and import** (glTF, FBX, Assimp, images, KTX2, file IO): `loader/` (`ModelImporter`, `AssetLoader` for
  files in the data directory, `ImageLoader`, `KtxLoader`, `MeshLoader`, `MaterialLoader`, `AnimationLoader`,
  `ShaderLoader`, `TerrainLoader`), `AssetReferences` (package-relative paths), Lua `AssetLoader`, `ModelImporter`,
  `ImportTextureFormat`, sampler settings `Wrapping` and `Filtering`. KTX2 encodings of imported textures are cached by
  content (`KtxLoader::EncodeImage`, `.cache/ktx2/`), a texture shared by models in several directories is encoded once.
- **Textures** (images, arrays, 3D textures, cube maps, atlases): `texture/` (`Texture2D`, `Texture2DArray`,
  `Texture3D`, `Cubemap`, `TextureAtlas`), CPU images `Image` (`common/Image`).
- **Streaming** (GPU memory budget, residency, LOD streaming, level streaming): `streaming/` (`StreamingManager`,
  `StreamingSettings`, `MeshStreamingSettings`, `LevelStreamingSettings`, `StreamingStats`, `ResidencyStats`), Lua
  types of the same names.

## Lua

- **Bindings** (scripting API, what scripts can call): `scripting/bindings/`, one file per area; every bound engine
  type is listed in this index. Math in `Glm` (`Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, `Quat`, `Translate`, `Rotate`,
  `Scale`, `LookAt`, `Mix`, `Clamp`, `Dot`, `EulerAngles`) with garbage free `*To` variants (`TranslateTo`, `MulTo`,
  `AddTo`, `CopyTo`, ...). Logging `Log`, job priorities `JobPriority`.

## Engine services

- **Jobs** (threads, background work, async, tasks): `jobsystem/` (`JobSystem`, `JobGroup`, priorities).
- **Profiling** (timings, CPU and GPU profiler, hitches, frame time, stutter): `profiler/` (`CPUProfiler`,
  `GPUProfiler`, `HitchRecorder`), `PerformanceCounter`, editor `ProfilerWindow`, ImGui `ProfilerPanel`,
  `PerformanceGraphPanel`.
- **Hitch recording** (spikes, stutter, slow frames, frame time outliers, causes): `HitchRecorder` detects CPU hitches
  (frame time, main or render thread) and GPU hitches (busiest queue, frame span) by a minimum time and/or a factor of
  the running average, keeps their profile and the queries that took longer than on average, `Evaluate` merges those
  causes; "Hitches" tab of the `ProfilerPanel` (settings saved in the editor config), `--log-hitches`, capture `hitches`.
- **Logging**: `Log` (C++ and Lua, `Log::printToConsole` switches the console output), editor `LogWindow`.
- **Running engine applications** (who uses the GPU, process list, busy, wait for the GPU): capture `InstanceRegistry`,
  the capture tool and games linking the capture library register themselves with what they do (not the editor and
  demo); capture `status`, every scene load waits until no other application is busy (`--no-wait-gpu`, job key
  `waitGpu`), `kill`.
- **Command line tools without the GPU** (modes before the engine starts): `EngineConfig::commandLineTool`.

## Common helpers

Search here before writing math or utility code (`common/`):

- **Math** (clamp, saturate, normalize, distance to segment, point in polygon, average, sum): `MathHelper`.
- **Random and noise** (random numbers, Perlin, Worley, uniform noise, sampling distributions): `RandomHelper`,
  `NoiseGenerator`, `Noise`, `Piecewise` (1D and 2D importance sampling), the low discrepancy `HaltonSequence`.
- **Hashing**: `Hash` (`HashCombine`).
- **Paths and strings** (file names, extensions, relative paths, wildcards): `Path`, `StringHelper` (`MatchWildcard`).
- **Colors** (sRGB, linear, gamma): `ColorConverter`.
- **Packing** (normals, 10-10-10-2): `Packing`.
- **Serialization** (JSON, optional fields, backwards compatible loading): `SerializationHelper` (`try_get_json`),
  `*Serializer` files next to each feature. Scenes store an entity table (ids, parents) and a section per component
  type, prefabs nest their entities (`Serializer::NestSceneEntities` converts scenes for tools). Scene setting `binaryColumns`
  (editor scene properties, Lua `scene.binaryColumns`): binary columns of raw values for the common components
  (`ComponentColumns`), about twice as fast to load. Binary scenes (MessagePack,
  resource table, matrices as raw floats, fast loading) and readable JSON scenes: `SerializationContext`
  (`WritePath`, `ReadHandle`, `WriteMatrix`), `AssetReferences::InlineResourceTable` for tools that copy entities out
  of a binary scene.
- **Shared pointers**: `Ref` (`Ref<T>`, `CreateRef`).
- **Filters** (Gaussian, box blur kernels): `Filter`.

## Shader helpers

`data/shader/common/` holds the shared GLSL helpers: random numbers, interleaved gradient and blue noise, packing and
normal encoding, octahedral mapping, color conversion (`common/convert.hsh`, `common/ycocg.hsh`), screen space tracing
(`common/traceScreenSpace.hsh`), edge preserving filters, sampling and tiling. Lighting models are in `brdf/`, ray
tracing surfaces in `raytracer/`, wind in `wind.hsh`.

## Editor

- **Windows**: `SceneWindow` (viewport, gizmos), `ContentBrowserWindow` (assets), `AssetEditorWindow` (an asset in its
  own scene), `MeshEditorWindow`, `MaterialEditorWindow`, `GeometryBrushWindow`, `ProfilerWindow`, `LogWindow`,
  `SettingsWindow` (engine settings), `DrawerWindow` (sliding panels), all derived from `Window`.
- **Tools**: `TerrainGenerator`, `VegetationGenerator`, `Guizmo`, `PrimitiveBatchWrapper`, `CopyPasteHelper`
  (entities and components), `FileSystemHelper`, `ResourcePayloadHelper` (drag and drop), `ToolSerializer`.
- **UI building blocks**: `ui/Widgets` (`DragVec3`, `Callout`, `IconButton`, ...), `Theme` colors, commands in
  `App::RegisterCommands`, undo in `SceneHistory`. Engine-aware ImGui panels in `libs/ImguiExtension` (`Panel` base,
  `StringHelper`), which games can use for debug UI.
- **Property search** (filter, find setting, search components, property table): `ImguiExtension::UIElements`
  `BeginPropertySearch`/`EndPropertySearch` show the rows of `BeginProperties` tables whose label, tooltip,
  `PropertySection` or nested `BeginPropertyGroup`s (components, list elements, materials) contain every search word.
  In the `BeginPropertyContent` of a group everything else (buttons, lists) is hidden unless it's a matching
  `BeginPropertyBlock`, groups without matches are hidden. `PropertySearchBar` is the input, `MatchesSearch` the shared
  case-insensitive matcher. The entity properties panel searches all components (Ctrl+F in the scene window).

## Capture tool (`src/capture`)

Renders, plays, measures, checks and edits scenes from JSON jobs, details in `capture/README.md`. A library
(`AtlasEngineCaptureLibrary`) and the `AtlasEngineCapture` executable.

- **Running**: jobs, the client `run`/`eval`/`wait`/`status`/`cancel`/`stop` of a server that keeps the scene loaded
  (`--serve`), `--pipeline` (scene builds from edit steps with snapshots), `--no-shots`, `--set`, `--validation`,
  `--streaming`, `--shader-dir`, `--no-wait-gpu`, `--foreground` (windows open in the background by default).
- **Without loading** (offline, no GPU, scene file, mesh file, model file, glTF, FBX, pivot, part offsets): `check`
  (job mistakes, entity names with the closest ones, Lua, preflight of `capture.project.json`), `inspect` (scene,
  prefab, mesh, model and material graph files, `--assembly`), `diff` (images as numbers), `compose` (contact sheets), `kill`.
- **Games** (test the game with its interface, HUD, menus, mouse): `HostedJob` runs a job in a game host
  (`--capture <job.json>`), captures of the window. Each build passes a job with `playerDisplay` (fullscreen, vsync
  of the player) before jobs in a window run.
- **Benchmarks** (A/B, performance comparison, timings): `bench` with builds or shader directories, interleaved rounds.
- **Cameras**: `scene`, `manual`, `focus` (an entity from named views), `follow`, `overview`.
- **Timeline events**: `capture` (with `crop`), `key`, `controller`, `mouse` (clicks in window pixels), `lua` (Lua in
  the scene's script state, `result` into the report), `look`, `teleport`, `log`, `expect` (properties or Lua),
  `resetScripts`, `pause`, `end`, `animation`, with conditions on log lines, Lua or properties (`when`).
- **Checks**: `grounded` (below the bounds center, `footprint` over a grid of the underside), `penetration`,
  `invalidTransforms`, `logErrors`, `failFast` (end at the first error), `compare` (images against a reference).
- **Queries**: `raycast`, `ground`, `entity`, `around`, `clearance`, `pick` (what is visible at a pixel), `path`
  (walkability along points), `route` (path search, optionally on the navigation mesh), `terrain`, `terrainSample`,
  `terrainMap` (map images with contours), `buried`, `materialGraphShaders`, `surfaceGrid` (floors, ceilings, decks,
  terrain in rooms), `corridor` (what blocks a road or path), `collisionCoverage` (meshes without collision).
- **Edit operations**: transforms `setTransform`, `translate`, `rotate`, `snapToGround`; entities `add` (meshes,
  lights, scripts, animated models), `duplicate`, `rename`, `delete`, `setVisible`; placement `scatter`, `clearArea`,
  `prefab`, `makePrefab`, `replaceWithPrefab`, `replaceMesh`; physics `meshBodies`, `trigger`, `constraint`; assets
  `meshSettings`, `meshCopy`, `meshMaterials`, `material`, `materialCopy`, `bakeMaterial`, `compressTextures`; terrain
  `terrain`, `terrainCreate`, `terrainCopy`, `shoreLine`, `detachTerrain`.
- **Measurements**: `profile` (GPU passes), hitches and `gpuMemory` in `report.json`, `sheet.jpg` of every job.
