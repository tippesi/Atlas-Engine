# Vulkan, pipelines and GPU data

What to keep in mind when changing GPU objects, pipelines, descriptor bindings, GPU structs or shaders, and when
debugging a GPU crash. Feature specific notes are in `wiki/EngineNotes.md`.

## GPU lifetime and pipelines

- `GraphicsDevice` keeps a reference to every created GPU object (`pipelines`, `buffers`, ...). Objects whose only remaining reference is the device's are handed to the `MemoryManager` and destroyed `framesToDeletion` (3) frames later, with `FRAME_DATA_COUNT` (2) frames in flight. Dropping a `Ref` is safe; don't add per-command-list lifetime tracking for pipelines or other objects.
- `PipelineManager` caches pipelines per shader and variant hash and creates them lazily on first use. In non-release builds shader hot reload is active and clears the pipeline variants of changed shaders.
- Shader compilation and the pipeline cache are used from several threads. The SPIR-V cache in `ShaderCompiler`, `Shader::GetVariant` and the `PipelineManager` map are guarded by locks; keep the lock order `variantsMutex` -> `variantMutex`, and keep hot reload skipping incomplete cache entries.
- A cache entry counts as fresh when its file is not older than the shader and its includes. A process writes the
  entries it compiled when it exits and gives the files the time of their sources, so a process which ran while a
  shader changed doesn't leave its old SPIR-V looking newer. Empty `data/.cache` and `bin/<config>/.cache` after a
  compiler update, which doesn't change the sources.
- Never write into shared create infos, e.g. `ShaderVariant::stageCreateInfos`. Extend a local copy (see `requiredSubgroupSize` in `Pipeline`), and never chain pointers to stack variables into data that outlives the call.
- `DescriptorPool` checks layouts against the capacity of its pools (`poolSizes`). Every new pool costs about 1 ms, a stutter that grows with the number of pipeline switches points there.
- Extension structs passed to Vulkan need the matching feature enabled in `GraphicsDevice` (e.g. `subgroupSizeControl` for `VkPipelineShaderStageRequiredSubgroupSizeCreateInfo`).
- Passes are recorded with dynamic rendering (Vulkan 1.3 is required): `graphics/Rendering.h` has `RenderingDesc` (attachment images, layer, load op, output layout, write enable, clear values), passed by value to `CommandList::BeginRendering`/`EndRendering`; there are no render pass or frame buffer objects. Pipelines take `AttachmentFormats(desc)` in `GraphicsPipelineDesc::attachmentFormats`. Clears go through `VK_ATTACHMENT_LOAD_OP_CLEAR`. `CommandList::AttachmentBarriers` transitions from the tracked `image->layout` (UNDEFINED for cleared attachments) and into `outputLayout`. `PipelineConfig::CalculateVariantHash` hashes the formats plus the constant barrier masks so draw sort order stays as it was; write enables aren't hashed. Only the swap chain keeps a `VkRenderPass` (`BeginRenderPass`/`EndRenderPass`), which ImGui uses.
- `GraphicsPipelineDesc::vertexInputInfo` points into member vectors of `VertexArray`, which `GetVertexInputState()` refills on every call. Configs that are copied and used later (e.g. `GetReversedWindingVariant()`) rely on the vertex array staying unchanged.
- Variant hashes don't include every pipeline state. When a new piece of state distinguishes pipelines (as `frontFace` does), add it to `PipelineConfig::CalculateVariantHash`.
- Validation warnings must not crash the application.
- Distinguish `src/engine/buffer/` (higher-level vertex/uniform abstractions) from `src/engine/graphics/Buffer.*` (low-level Vulkan buffer wrapper).

## GPU data layout

- Uniform structs on the C++ side use `struct alignas(16)` and explicit `padding0`, `padding1`, ... members, mirrored by the GLSL `std140` block. Keep member order identical on both sides.
- Descriptor bindings are shared between C++ and many shaders. Before adding or moving one, search both `data/shader` and `src/engine` for the set/binding. Notable fixed slots: set 1 binding 17 is the clamp-to-edge sampler, set 1 binding 18 the light buffer, set 2 binding 27 the DDGI uniform buffer, set 2 binding 28 the last instance matrices, set 1 bindings 20-23 the GPU scene instances, history, instance indices and impostor indices (bindless).

## Shaders

- Shaders look like the C++ code: `PascalCase` functions, `camelCase` variables and file names, the same braces and empty lines, float literals with a decimal point. Bindings are written `layout(set = 3, binding = 0)`, push constants `layout(push_constant) uniform constants { ... } pushConstants;`, includes `#include <../globals.hsh>`.
- Identical shader blocks become a helper in a shared `.hsh`. When a change applies to a feature with several shaders (ocean surface and underwater, impostor color and shadow), change all of them.
- Shaders are compiled at runtime, so a successful C++ build says nothing about them. To check a changed shader, expand its `#include`s the way `loader/ShaderLoader.cpp` does (relative to the including file, each file once, `#extension` lines hoisted to the top, `#version 460` plus macro defines prepended), then run `glslangValidator -V --target-env vulkan1.3 -S <stage>` and `spirv-val` from the Vulkan SDK for the relevant macro combinations.

## Debugging GPU crashes

- An access violation inside `nvgpucomp64.dll`/`nvoglv64.dll` on a driver thread, seconds after the last pipeline creation, is the NVIDIA driver compiling or optimizing a pipeline in the background. Only driver frames are on the stack. It can be a driver bug even for valid SPIR-V; reproduce it with an empty shader cache (`data/.cache/` plus the driver's cache).
- First check the SPIR-V of the involved shaders with `spirv-val`, run with validation layers, and make sure create infos don't reference freed or changing data before suspecting object lifetimes.
- A lost device only shows where the CPU noticed it. Start the application with `--crash-diagnostics` (editor, game,
  bridge): the log then names the profiler query the GPU was in and the faulting addresses. To find the shader, return
  early from parts of it and start again, shaders are compiled at runtime.
- Set 1 is shared: geometry passes bind their instance and joint buffers where the deferred passes read the G-buffer
  (bindings 1 to 5). Geometry drawn after `MainRenderer` bound the G-buffer (the translucent passes) has to bind it again
  (`MainRenderer::BindGBuffer`), otherwise the deferred passes read buffers as images and the device is lost.
- Temporary diagnostics (logging, frame tracking) must be marked clearly and removed again before committing.
