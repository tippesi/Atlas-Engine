#include "MainRenderer.h"
#include "helper/GeometryHelper.h"
#include "helper/HaltonSequence.h"

#include "../common/Packing.h"
#include "../tools/PerformanceCounter.h"
#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void MainRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            CreateGlobalDescriptorSetLayout();

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);
            Helper::GeometryHelper::GenerateCubeVertexArray(cubeVertexArray);

            haltonSequence = Helper::HaltonSequence::Generate(2, 3, 16 + 1);

            PreintegrateBRDF();

            auto uniformBufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .hostAccess = Graphics::BufferHostAccess::Sequential,
                .size = sizeof(GlobalUniforms),
            };
            globalUniformBuffer = device->CreateMultiBuffer(uniformBufferDesc);
            pathTraceGlobalUniformBuffer = device->CreateMultiBuffer(uniformBufferDesc);

            uniformBufferDesc.size = sizeof(DDGIUniforms);
            ddgiUniformBuffer = device->CreateMultiBuffer(uniformBufferDesc);

            opaqueRenderer.Init(device);
            impostorRenderer.Init(device);
            terrainRenderer.Init(device);
            shadowRenderer.Init(device);
            impostorShadowRenderer.Init(device);
            terrainShadowRenderer.Init(device);
            gBufferRenderer.Init(device);
            ddgiRenderer.Init(device);
            rtgiRenderer.Init(device);
            ssgiRenderer.Init(device);
            aoRenderer.Init(device);
            rtrRenderer.Init(device);
            sssRenderer.Init(device);
            directLightRenderer.Init(device);
            indirectLightRenderer.Init(device);
            skyboxRenderer.Init(device);
            atmosphereRenderer.Init(device);
            oceanRenderer.Init(device);
            volumetricCloudRenderer.Init(device);
            volumetricRenderer.Init(device);
            taaRenderer.Init(device);
            postProcessRenderer.Init(device);
            pathTracingRenderer.Init(device);
            fsr2Renderer.Init(device);
            textRenderer.Init(device);
            textureRenderer.Init(device);

            font = Atlas::CreateRef<Atlas::Font>("font/roboto.ttf", 22.0f, 5);

        }

        void MainRenderer::RenderScene(Ref<Viewport> viewport, Ref<RenderTarget> target, Ref<Scene::Scene> scene,
            Ref<PrimitiveBatch> primitiveBatch, Texture::Texture2D* texture) {

            if (!device->swapChain->isComplete || !scene->HasMainCamera())
                return;

            auto& camera = scene->GetMainCamera();
            auto commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue);

            commandList->BeginCommands();

            auto& taa = scene->postProcessing.taa;
            if (taa.enable || scene->postProcessing.fsr2) {
                vec2 jitter = vec2(0.0f);
                if (scene->postProcessing.fsr2) {
                    jitter = fsr2Renderer.GetJitter(target, frameCount);
                }
                else {
                    jitter = 2.0f * haltonSequence[haltonIndex] - 1.0f;
                    jitter.x /= (float)target->GetScaledWidth();
                    jitter.y /= (float)target->GetScaledHeight();
                }

                camera.Jitter(jitter * taa.jitterRange);
            }
            else {
                // Even if there is no TAA we need to update the jitter for other techniques
                // E.g. the reflections and ambient occlusion use reprojection
                camera.Jitter(vec2(0.0f));
            }

            auto sceneState = &scene->renderState;

            Graphics::Profiler::BeginThread("Main renderer", commandList);
            Graphics::Profiler::BeginQuery("Render scene");

            JobGroup fillRenderListGroup { JobPriority::High };
            JobSystem::Execute(fillRenderListGroup, [&](JobData&) { FillRenderList(scene, camera); });

            lightData.CullAndSort(scene);

            SetUniforms(target, scene, camera);

            commandList->BindBuffer(globalUniformBuffer, 1, 31);
            commandList->BindImage(dfgPreintegrationTexture.image, dfgPreintegrationTexture.sampler, 1, 12);
            commandList->BindSampler(globalSampler, 1, 13);
            commandList->BindSampler(globalNearestSampler, 1, 15);

            if (scene->clutter)
                vegetationRenderer.helper.PrepareInstanceBuffer(*scene->clutter, camera, commandList);

            if (scene->ocean && scene->ocean->enable)
                scene->ocean->simulation.Compute(commandList);

            if (scene->sky.probe) {
                if (scene->sky.probe->update) {
                    FilterProbe(scene->sky.probe, commandList);
                    //scene->sky.probe->update = false;
                }
            }
            else if (scene->sky.atmosphere) {
                atmosphereRenderer.Render(scene->sky.atmosphere->probe, scene, commandList);
                FilterProbe(scene->sky.atmosphere->probe, commandList);
            }

            if (scene->irradianceVolume) {
                commandList->BindBuffer(ddgiUniformBuffer, 2, 26);
            }

            volumetricCloudRenderer.RenderShadow(target, scene, commandList);

            Log::Warning("Begin wait material");
            JobSystem::WaitSpin(sceneState->materialUpdateJob);
            Log::Warning("End wait material");
            sceneState->materialBuffer.Bind(commandList, 1, 14);

            // Wait as long as possible for this to finish
            Log::Warning("Begin wait bindless");
            JobSystem::WaitSpin(sceneState->bindlessMeshMapUpdateJob);
            JobSystem::WaitSpin(sceneState->bindlessTextureMapUpdateJob);
            JobSystem::WaitSpin(sceneState->prepareBindlessMeshesJob);
            JobSystem::WaitSpin(sceneState->prepareBindlessTexturesJob);
            Log::Warning("End wait bindless");
            lightData.UpdateBindlessIndices(scene);
            commandList->BindBuffers(sceneState->triangleBuffers, 0, 1);
            if (sceneState->images.size())
                commandList->BindSampledImages(sceneState->images, 0, 3);

            if (device->support.hardwareRayTracing) {
                commandList->BindBuffers(sceneState->triangleOffsetBuffers, 0, 2);
            }
            else {
                commandList->BindBuffers(sceneState->blasBuffers, 0, 0);
                commandList->BindBuffers(sceneState->bvhTriangleBuffers, 0, 2);
            }

            {
                shadowRenderer.Render(target, scene, commandList, &renderList);

                terrainShadowRenderer.Render(target, scene, commandList);
            }

            if (scene->sky.GetProbe()) {
                commandList->BindImage(scene->sky.GetProbe()->filteredSpecular.image,
                    scene->sky.GetProbe()->filteredSpecular.sampler, 1, 10);
                commandList->BindImage(scene->sky.GetProbe()->filteredDiffuse.image,
                    scene->sky.GetProbe()->filteredDiffuse.sampler, 1, 11);
            }

            {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers;

                auto lightSubset = scene->GetSubset<LightComponent>();

                for (auto& lightEntity : lightSubset) {
                    auto& light = lightEntity.GetComponent<LightComponent>();
                    if (!light.shadow || !light.shadow->update)
                        continue;

                    auto shadow = light.shadow;
                    imageBarriers.push_back({ shadow->useCubemap ?
                        shadow->cubemap->image : shadow->maps->image, layout, access });
                }

                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
            }

            Log::Warning("Begin wait ray tracing");
            JobSystem::WaitSpin(scene->renderState.rayTracingWorldUpdateJob);
            Log::Warning("End wait ray tracing");

            lightData.FillBuffer(scene);
            lightData.lightBuffer.Bind(commandList, 1, 16);

            ddgiRenderer.TraceAndUpdateProbes(scene, commandList);

            // Only here does the main pass need to be ready
            JobSystem::Wait(fillRenderListGroup);

            {
                Graphics::Profiler::BeginQuery("Main render pass");

                commandList->BeginRenderPass(target->gBufferRenderPass, target->gBufferFrameBuffer, true);

                opaqueRenderer.Render(target, scene, commandList, &renderList, sceneState->materialMap);

                ddgiRenderer.DebugProbes(target, scene, commandList, sceneState->materialMap);

                vegetationRenderer.Render(target, scene, commandList, sceneState->materialMap);

                terrainRenderer.Render(target, scene, commandList, sceneState->materialMap);

                impostorRenderer.Render(target, scene, commandList, &renderList, sceneState->materialMap);

                commandList->EndRenderPass();

                Graphics::Profiler::EndQuery();
            }

            oceanRenderer.RenderDepthOnly(target, scene, commandList);

            auto targetData = target->GetData(FULL_RES);

            commandList->BindImage(targetData->baseColorTexture->image, targetData->baseColorTexture->sampler, 1, 3);
            commandList->BindImage(targetData->normalTexture->image, targetData->normalTexture->sampler, 1, 4);
            commandList->BindImage(targetData->geometryNormalTexture->image, targetData->geometryNormalTexture->sampler, 1, 5);
            commandList->BindImage(targetData->roughnessMetallicAoTexture->image, targetData->roughnessMetallicAoTexture->sampler, 1, 6);
            commandList->BindImage(targetData->materialIdxTexture->image, targetData->materialIdxTexture->sampler, 1, 7);
            commandList->BindImage(targetData->depthTexture->image, targetData->depthTexture->sampler, 1, 8);

            if (!target->HasHistory()) {
                auto rtHalfData = target->GetHistoryData(HALF_RES);
                auto rtData = target->GetHistoryData(FULL_RES);
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers = {
                    {rtData->baseColorTexture->image, layout, access},
                    {rtData->depthTexture->image, layout, access},
                    {rtData->normalTexture->image, layout, access},
                    {rtData->geometryNormalTexture->image, layout, access},
                    {rtData->roughnessMetallicAoTexture->image, layout, access},
                    {rtData->offsetTexture->image, layout, access},
                    {rtData->materialIdxTexture->image, layout, access},
                    {rtData->stencilTexture->image, layout, access},
                    {rtData->velocityTexture->image, layout, access},
                    {rtHalfData->baseColorTexture->image, layout, access},
                    {rtHalfData->depthTexture->image, layout, access},
                    {rtHalfData->normalTexture->image, layout, access},
                    {rtHalfData->geometryNormalTexture->image, layout, access},
                    {rtHalfData->roughnessMetallicAoTexture->image, layout, access},
                    {rtHalfData->offsetTexture->image, layout, access},
                    {rtHalfData->materialIdxTexture->image, layout, access},
                    {rtHalfData->stencilTexture->image, layout, access},
                    {rtHalfData->velocityTexture->image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            {
                auto rtData = target->GetData(FULL_RES);

                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers = {
                    {rtData->baseColorTexture->image, layout, access},
                    {rtData->depthTexture->image, layout, access},
                    {rtData->normalTexture->image, layout, access},
                    {rtData->geometryNormalTexture->image, layout, access},
                    {rtData->roughnessMetallicAoTexture->image, layout, access},
                    {rtData->offsetTexture->image, layout, access},
                    {rtData->materialIdxTexture->image, layout, access},
                    {rtData->stencilTexture->image, layout, access},
                    {rtData->velocityTexture->image, layout, access},
                    {target->oceanStencilTexture.image, layout, access},
                    {target->oceanDepthTexture.image, layout, access}
                };

                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
            }

            {
                if (scene->sky.probe) {
                    skyboxRenderer.Render(target, scene, commandList);
                }
                else if (scene->sky.atmosphere) {
                    atmosphereRenderer.Render(target, scene, commandList);
                }
            }

            gBufferRenderer.FillNormalTexture(target, commandList);

            gBufferRenderer.Downscale(target, commandList);

            aoRenderer.Render(target, scene, commandList);

            rtgiRenderer.Render(target, scene, commandList);

            rtrRenderer.Render(target, scene, commandList);

            sssRenderer.Render(target, scene, commandList);

            {
                Graphics::Profiler::BeginQuery("Lighting pass");

                commandList->ImageMemoryBarrier(target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                directLightRenderer.Render(target, scene, lightData, commandList);

                if (!scene->rtgi || !scene->rtgi->enable || !scene->IsRtDataValid()) {
                    commandList->ImageMemoryBarrier(target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                    ssgiRenderer.Render(target, scene, commandList);
                }

                commandList->ImageMemoryBarrier(target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                indirectLightRenderer.Render(target, scene, commandList);

                Graphics::ImageBarrier outBarrier(target->lightingTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                commandList->ImageMemoryBarrier(outBarrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                Graphics::Profiler::EndQuery();
            }

            // This was needed after the ocean renderer, if we ever want to have alpha transparency we need it again
            // downscaleRenderer.Downscale(target, commandList);

            {
                commandList->BeginRenderPass(target->afterLightingRenderPass, target->afterLightingFrameBuffer);

                textRenderer.Render(target, scene, commandList);
                
                commandList->EndRenderPass();

                auto rtData = target->GetData(FULL_RES);

                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers = {
                    {target->lightingTexture.image, layout, access},
                    {rtData->depthTexture->image, layout, access},
                    {rtData->velocityTexture->image, layout, access},
                    {rtData->stencilTexture->image, layout, access},
                };

                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
            
            }

            {
                volumetricCloudRenderer.Render(target, scene, commandList);

                volumetricRenderer.Render(target, scene, commandList);
            }

            oceanRenderer.Render(target, scene, commandList);

            if (primitiveBatch)
                RenderPrimitiveBatch(viewport, target, primitiveBatch, scene->GetMainCamera(), commandList);

            {
                if (scene->postProcessing.fsr2) {
                    gBufferRenderer.GenerateReactiveMask(target, commandList);

                    fsr2Renderer.Render(target, scene, commandList);
                }
                else {
                    taaRenderer.Render(target, scene, commandList);
                }

                target->Swap();

                postProcessRenderer.Render(target, scene, commandList, texture);
            }

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndThread();

            commandList->EndCommands();
            device->SubmitCommandList(commandList);

            renderList.Clear();

        }

        void MainRenderer::PathTraceScene(Ref<Viewport> viewport, Ref<PathTracerRenderTarget> target,
            Ref<Scene::Scene> scene, Texture::Texture2D *texture) {

            if (!scene->IsRtDataValid() || !device->swapChain->isComplete || !scene->HasMainCamera())
                return;

            auto sceneState = &scene->renderState;

            static vec2 lastJitter = vec2(0.0f);

            auto& camera = scene->GetMainCamera();

            auto commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue);

            auto jitter = 2.0f * haltonSequence[haltonIndex] - 1.0f;
            jitter.x /= (float)target->GetWidth();
            jitter.y /= (float)target->GetHeight();

            camera.Jitter(jitter * 0.0f);

            commandList->BeginCommands();

            Graphics::Profiler::BeginThread("Path tracing", commandList);
            Graphics::Profiler::BeginQuery("Buffer operations");

            auto globalUniforms = GlobalUniforms{
                 .vMatrix = camera.viewMatrix,
                 .pMatrix = camera.projectionMatrix,
                 .ivMatrix = camera.invViewMatrix,
                 .ipMatrix = camera.invProjectionMatrix,
                 .pvMatrixLast = camera.GetLastJitteredMatrix(),
                 .pvMatrixCurrent = camera.projectionMatrix * camera.viewMatrix,
                 .jitterLast = lastJitter,
                 .jitterCurrent = jitter,
                 .cameraLocation = vec4(camera.GetLocation(), 0.0f),
                 .cameraDirection = vec4(camera.direction, 0.0f),
                 .cameraUp = vec4(camera.up, 0.0f),
                 .cameraRight = vec4(camera.right, 0.0f),
                 .planetCenter = vec4(scene->sky.planetCenter, 0.0f),
                 .planetRadius = scene->sky.planetRadius,
                 .time = Clock::Get(),
                 .deltaTime = Clock::GetDelta(),
                 .frameCount = frameCount
            };

            lastJitter = jitter;

            pathTraceGlobalUniformBuffer->SetData(&globalUniforms, 0, sizeof(GlobalUniforms));

            JobSystem::WaitSpin(scene->renderState.rayTracingWorldUpdateJob);
            JobSystem::WaitSpin(sceneState->prepareBindlessMeshesJob);
            JobSystem::WaitSpin(sceneState->prepareBindlessTexturesJob);

            commandList->BindBuffer(pathTraceGlobalUniformBuffer, 1, 31);
            commandList->BindImage(dfgPreintegrationTexture.image, dfgPreintegrationTexture.sampler, 1, 12);
            commandList->BindSampler(globalSampler, 1, 13);
            commandList->BindSampler(globalNearestSampler, 1, 15);
            commandList->BindBuffers(sceneState->triangleBuffers, 0, 1);
            if (sceneState->images.size())
                commandList->BindSampledImages(sceneState->images, 0, 3);

            if (device->support.hardwareRayTracing) {
                commandList->BindBuffers(sceneState->triangleOffsetBuffers, 0, 2);
            }
            else {
                commandList->BindBuffers(sceneState->blasBuffers, 0, 0);
                commandList->BindBuffers(sceneState->bvhTriangleBuffers, 0, 2);
            }


            Graphics::Profiler::EndQuery();

            // No probe filtering required
            if (scene->sky.atmosphere) {
                atmosphereRenderer.Render(scene->sky.atmosphere->probe, scene, commandList);
            }

            pathTracingRenderer.Render(target, scene, ivec2(1, 1), commandList);

            if (pathTracingRenderer.realTime) {
                taaRenderer.Render(target, scene, commandList);

                postProcessRenderer.Render(target, scene, commandList, texture);
            }
            else {
                Graphics::Profiler::BeginQuery("Post processing");

                if (device->swapChain->isComplete) {
                    commandList->BeginRenderPass(device->swapChain, true);

                    textureRenderer.RenderTexture2D(commandList, viewport, &target->texture,
                        0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0f, 1.0f, false, true);

                    commandList->EndRenderPass();
                }

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::EndThread();

            commandList->EndCommands();

            device->SubmitCommandList(commandList);

        }

        void MainRenderer::RenderPrimitiveBatch(Ref<Viewport> viewport, Ref<RenderTarget> target,
            Ref<PrimitiveBatch> batch, const CameraComponent& camera, Graphics::CommandList* commandList) {

            bool localCommandList = !commandList;

            if (localCommandList) {
                commandList = device->GetCommandList(Graphics::GraphicsQueue);

                commandList->BeginCommands();
            }

            batch->TransferData();

            auto rtData = target->GetData(FULL_RES);

            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

            std::vector<Graphics::BufferBarrier> bufferBarriers;
            std::vector<Graphics::ImageBarrier> imageBarriers = {
                {target->lightingTexture.image, layout, access},
                {rtData->depthTexture->image, layout, access},
                {rtData->velocityTexture->image, layout, access},
            };

            commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            commandList->BeginRenderPass(target->afterLightingRenderPass, target->afterLightingFrameBuffer);

            if (batch->GetLineCount()) {

                auto pipelineConfig = GetPipelineConfigForPrimitives(target->afterLightingFrameBuffer,
                    batch->lineVertexArray, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, batch->testDepth);

                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                commandList->BindPipeline(pipeline);
                batch->lineVertexArray.Bind(commandList);

                commandList->SetLineWidth(batch->GetLineWidth());

                commandList->Draw(batch->GetLineCount() * 2);

            }


            if (batch->GetTriangleCount()) {

                auto pipelineConfig = GetPipelineConfigForPrimitives(target->afterLightingFrameBuffer,
                    batch->triangleVertexArray, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, batch->testDepth);

                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                commandList->BindPipeline(pipeline);
                batch->triangleVertexArray.Bind(commandList);

                commandList->Draw(batch->GetTriangleCount() * 3);

            }


            commandList->EndRenderPass();

            imageBarriers = {
                {target->lightingTexture.image, layout, access},
                {rtData->depthTexture->image, layout, access},
                {rtData->velocityTexture->image, layout, access},
                {rtData->stencilTexture->image, layout, access},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            if (localCommandList) {
                commandList->EndCommands();

                device->SubmitCommandList(commandList);
            }

        }

        void MainRenderer::RenderProbe(Ref<Lighting::EnvironmentProbe> probe, Ref<RenderTarget> target, Ref<Scene::Scene> scene) {

            /*
            if (probe->resolution != target->GetWidth() ||
                probe->resolution != target->GetHeight())
                return;

            std::vector<PackedMaterial> materials;
            std::unordered_map<void*, uint16_t> materialMap;
            Viewport viewport(0, 0, probe->resolution, probe->resolution);

            PrepareMaterials(scene, materials, materialMap);

            auto materialBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(PackedMaterial), 0,
                materials.size(), materials.data());            

            Lighting::EnvironmentProbe* skyProbe = nullptr;

            if (scene->sky.probe) {
                skyProbe = scene->sky.probe;
                scene->sky.probe = nullptr;
            }

            vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
                             vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                             vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

            vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                           vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
                           vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

            Camera camera(90.0f, 1.0f, 0.5f, 1000.0f);
            camera.UpdateProjection();

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            for (uint8_t i = 0; i < 6; i++) {

                vec3 dir = faces[i];
                vec3 up = ups[i];
                vec3 right = normalize(cross(up, dir));
                up = normalize(cross(dir, right));

                camera.viewMatrix = glm::lookAt(probe->GetPosition(), probe->GetPosition() + dir, up);
                camera.invViewMatrix = glm::inverse(camera.viewMatrix);
                camera.location = probe->GetPosition();
                camera.direction = dir;
                camera.right = right;
                camera.up = up;

                camera.frustum = Volume::Frustum(camera.projectionMatrix * camera.viewMatrix);

                scene->Update(&camera, 0.0f);

                // Clear the lights depth maps
                depthFramebuffer.Bind();

                auto lights = scene->GetLights();

                if (scene->sky.sun) {
                    lights.push_back(scene->sky.sun);
                }

                for (auto light : lights) {

                    if (!light->GetShadow())
                        continue;
                    if (!light->GetShadow()->update)
                        continue;

                    for (int32_t j = 0; j < light->GetShadow()->componentCount; j++) {
                        if (light->GetShadow()->useCubemap) {
                            depthFramebuffer.AddComponentCubemap(GL_DEPTH_ATTACHMENT,
                                &light->GetShadow()->cubemap, j);
                        }
                        else {
                            depthFramebuffer.AddComponentTextureArray(GL_DEPTH_ATTACHMENT,
                                &light->GetShadow()->maps, j);
                        }

                        glClear(GL_DEPTH_BUFFER_BIT);
                    }
                }

                shadowRenderer.Render(&viewport, target, &camera, scene);

                glEnable(GL_CULL_FACE);

                glCullFace(GL_FRONT);

                terrainShadowRenderer.Render(&viewport, target, &camera, scene);

                glCullFace(GL_BACK);

                // Shadows have been updated
                for (auto light : lights) {
                    if (!light->GetShadow())
                        continue;
                    light->GetShadow()->update = false;
                }

                materialBuffer.BindBase(0);

                target->geometryFramebuffer.Bind(true);
                target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
                    GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
                    GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 });

                glEnable(GL_CULL_FACE);

                glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

                opaqueRenderer.Render(&viewport, target, &camera, scene, materialMap);

                terrainRenderer.Render(&viewport, target, &camera, scene, materialMap);

                glEnable(GL_CULL_FACE);
                glDepthMask(GL_FALSE);
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
                    GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

                decalRenderer.Render(&viewport, target, &camera, scene);

                glDisable(GL_BLEND);

                vertexArray.Bind();

                target->lightingFramebuffer.Bind(true);
                target->lightingFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0 });

                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                directionalLightRenderer.Render(&viewport, target, &camera, scene);

                glEnable(GL_DEPTH_TEST);

                target->lightingFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
                    GL_COLOR_ATTACHMENT1 });

                glDepthMask(GL_TRUE);

                oceanRenderer.Render(&viewport, target, &camera, scene);

                glDisable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                glDepthMask(GL_FALSE);
                glDisable(GL_DEPTH_TEST);

                vertexArray.Bind();

                volumetricRenderer.Render(&viewport, target, &camera, scene);

                createProbeFaceShader.Bind();

                createProbeFaceShader.GetUniform("faceIndex")->SetValue((int32_t)i);
                createProbeFaceShader.GetUniform("ipMatrix")->SetValue(camera.invProjectionMatrix);

                int32_t groupCount = probe->resolution / 8;
                groupCount += ((groupCount * 8 == probe->resolution) ? 0 : 1);

                probe->cubemap.Bind(GL_WRITE_ONLY, 0);
                probe->depth.Bind(GL_WRITE_ONLY, 1);

                target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(0);
                target->lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(1);

                glDispatchCompute(groupCount, groupCount, 1);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            }

            if (skyProbe) {
                scene->sky.probe = skyProbe;
            }
            */

        }

        void MainRenderer::FilterProbe(Ref<Lighting::EnvironmentProbe> probe, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Filter probe");

            mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
            vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
                             vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                             vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

            vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                           vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
                           vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

            Graphics::Profiler::BeginQuery("Filter diffuse probe");

            auto pipelineConfig = PipelineConfig("brdf/filterProbe.csh", { "FILTER_DIFFUSE" });
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

            commandList->BindPipeline(pipeline);

            //auto constantRange = pipeline->shader->GetPushConstantRange("constants");
            //commandList->PushConstants()

            // It's only accessed in compute shaders
            commandList->ImageMemoryBarrier(probe->filteredDiffuse.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            
            auto& cubemap = probe->GetCubemap();
            if (cubemap.image->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                commandList->ImageMemoryBarrier(cubemap.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            commandList->BindImage(probe->filteredDiffuse.image, 3, 0);
            commandList->BindImage(cubemap.image, cubemap.sampler, 3, 1);

            ivec2 res = ivec2(probe->filteredDiffuse.width, probe->filteredDiffuse.height);
            ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
            groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 6);

            commandList->ImageMemoryBarrier(probe->filteredDiffuse.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            Graphics::Profiler::EndAndBeginQuery("Filter specular probe");

            pipelineConfig = PipelineConfig("brdf/filterProbe.csh", { "FILTER_SPECULAR" });
            pipeline = PipelineManager::GetPipeline(pipelineConfig);

            struct alignas(16) PushConstants {
                int cubeMapMipLevels;
                float roughness;
                uint32_t mipLevel;
            };

            commandList->BindPipeline(pipeline);

            commandList->ImageMemoryBarrier(probe->filteredSpecular.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            int32_t width = int32_t(probe->filteredSpecular.width);
            int32_t height = int32_t(probe->filteredSpecular.height);

            for (uint32_t i = 0; i < probe->filteredSpecular.image->mipLevels; i++) {
                Graphics::Profiler::BeginQuery("Mip level " + std::to_string(i));

                ivec2 res = ivec2(width, height);

                commandList->BindImage(probe->filteredSpecular.image, 3, 0, i);

                PushConstants pushConstants = {
                    .cubeMapMipLevels = int32_t(probe->GetCubemap().image->mipLevels),
                    .roughness = float(i) / float(probe->filteredSpecular.image->mipLevels - 1),
                    .mipLevel = i
                };
                commandList->PushConstants("constants", &pushConstants);
               
                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 6);

                width /= 2;
                height /= 2;

                Graphics::Profiler::EndQuery();

            }

            commandList->ImageMemoryBarrier(probe->filteredSpecular.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            Graphics::Profiler::EndQuery();

            Graphics::Profiler::EndQuery();

        }

        void MainRenderer::Update() {

            textRenderer.Update();

            haltonIndex = (haltonIndex + 1) % haltonSequence.size();
            frameCount++;

        }

        void MainRenderer::CreateGlobalDescriptorSetLayout() {

            if (!device->support.bindless)
                return;

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .maxLod = 12,
                .anisotropicFiltering = true
            };
            globalSampler = device->CreateSampler(samplerDesc);

            samplerDesc = Graphics::SamplerDesc{
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .maxLod = 12,
                .anisotropicFiltering = false
            };
            globalNearestSampler = device->CreateSampler(samplerDesc);

            auto layoutDesc = Graphics::DescriptorSetLayoutDesc{
                .bindings = {
                    {
                        .bindingIdx = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = 8192, .bindless = true
                    },
                    {
                        .bindingIdx = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = 8192, .bindless = true
                    },
                    {
                        .bindingIdx = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = 8192, .bindless = true
                    },
                    {
                        .bindingIdx = 3, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .descriptorCount = 16384, .bindless = true
                    }
                },
                .bindingCount = 4
            };
            globalDescriptorSetLayout = device->CreateDescriptorSetLayout(layoutDesc);

            PipelineManager::OverrideDescriptorSetLayout(globalDescriptorSetLayout, 0);

        }

        void MainRenderer::SetUniforms(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, const CameraComponent& camera) {

            auto globalUniforms = GlobalUniforms {
                .vMatrix = camera.viewMatrix,
                .pMatrix = camera.projectionMatrix,
                .ivMatrix = camera.invViewMatrix,
                .ipMatrix = camera.invProjectionMatrix,
                .pvMatrixLast = camera.GetLastJitteredMatrix(),
                .pvMatrixCurrent = camera.projectionMatrix * camera.viewMatrix,
                .ipvMatrixLast = glm::inverse(camera.GetLastJitteredMatrix()),
                .ipvMatrixCurrent = glm::inverse(camera.projectionMatrix * camera.viewMatrix),
                .jitterLast = camera.GetLastJitter(),
                .jitterCurrent = camera.GetJitter(),
                .cameraLocation = vec4(camera.GetLocation(), 0.0f),
                .cameraDirection = vec4(camera.direction, 0.0f),
                .cameraUp = vec4(camera.up, 0.0f),
                .cameraRight = vec4(camera.right, 0.0f),
                .planetCenter = vec4(scene->sky.planetCenter, 0.0f),
                .windDir = glm::normalize(scene->wind.direction),
                .windSpeed = scene->wind.speed,
                .planetRadius = scene->sky.planetRadius,
                .time = Clock::Get(),
                .deltaTime = Clock::GetDelta(),
                .frameCount = frameCount,
                .mipLodBias = -1.0f / target->GetScalingFactor()
            };

            auto frustumPlanes = camera.frustum.GetPlanes();
            std::copy(frustumPlanes.begin(), frustumPlanes.end(), &globalUniforms.frustumPlanes[0]);

            globalUniformBuffer->SetData(&globalUniforms, 0, sizeof(GlobalUniforms));

            if (scene->irradianceVolume) {
                auto volume = scene->irradianceVolume;

                if (volume->scroll) {
                    //auto pos = vec3(0.4f, 12.7f, -43.0f);
                    //auto pos = glm::vec3(30.0f, 25.0f, 0.0f);
                    auto pos = camera.GetLocation();

                    auto volumeSize = volume->aabb.GetSize();
                    auto volumeAABB = Volume::AABB(-volumeSize / 2.0f + pos, volumeSize / 2.0f + pos);
                    volume->SetAABB(volumeAABB);
                }

                auto probeCountPerCascade = volume->probeCount.x * volume->probeCount.y * 
                    volume->probeCount.z;
                auto ddgiUniforms = DDGIUniforms {
                    .volumeCenter = vec4(camera.GetLocation(), 1.0f),
                    .volumeProbeCount = ivec4(volume->probeCount, probeCountPerCascade),
                    .cascadeCount = volume->cascadeCount,
                    .volumeBias = volume->bias,
                    .volumeIrradianceRes = volume->irrRes,
                    .volumeMomentsRes = volume->momRes,
                    .rayCount = volume->rayCount,
                    .inactiveRayCount = volume->rayCountInactive,
                    .hysteresis = volume->hysteresis,
                    .volumeGamma = volume->gamma,
                    .volumeStrength = volume->strength,
                    .depthSharpness = volume->sharpness,
                    .optimizeProbes = volume->optimizeProbes ? 1 : 0,
                    .volumeEnabled = volume->enable ? 1 : 0
                };

                for (int32_t i = 0; i < volume->cascadeCount; i++) {
                    ddgiUniforms.cascades[i] = DDGICascade{
                        .volumeMin = vec4(volume->cascades[i].aabb.min, 1.0f),
                        .volumeMax = vec4(volume->cascades[i].aabb.max, 1.0f),
                        .cellSize = vec4(volume->cascades[i].cellSize, glm::length(volume->cascades[i].cellSize)),
                        .offsetDifference = ivec4(volume->cascades[i].offsetDifferences, 0),
                    };
                }

                ddgiUniformBuffer->SetData(&ddgiUniforms, 0, sizeof(DDGIUniforms));
            }
            else {
                auto ddgiUniforms = DDGIUniforms {
                    .volumeEnabled = 0
                };

                for (int32_t i = 0; i < MAX_IRRADIANCE_VOLUME_CASCADES; i++) {
                    ddgiUniforms.cascades[i] = DDGICascade {
                        .volumeMin = vec4(0.0f),
                        .volumeMax = vec4(0.0f),
                        .cellSize = vec4(0.0f),
                    };
                }

                ddgiUniformBuffer->SetData(&ddgiUniforms, 0, sizeof(DDGIUniforms));
            }

            auto meshes = scene->GetMeshes();
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded() || !mesh->impostor) continue;

                auto impostor = mesh->impostor;
                Mesh::Impostor::ImpostorInfo impostorInfo = {
                    .center = vec4(impostor->center, 1.0f),
                    .radius = impostor->radius,
                    .views = impostor->views,
                    .cutoff = impostor->cutoff,
                    .mipBias = impostor->mipBias
                };

                impostor->impostorInfoBuffer.SetData(&impostorInfo, 0);
            }

        }

        void MainRenderer::FillRenderList(Ref<Scene::Scene> scene, const CameraComponent& camera) {

            auto meshes = scene->GetMeshes();
            renderList.NewFrame(scene);

            auto lightSubset = scene->GetSubset<LightComponent>();

            JobGroup group;
            for (auto& lightEntity : lightSubset) {

                auto& light = lightEntity.GetComponent<LightComponent>();
                if (!light.shadow || !light.shadow->update)
                    continue;

                auto& shadow = light.shadow;

                auto componentCount = shadow->longRange ?
                    shadow->viewCount - 1 : shadow->viewCount;

                JobSystem::ExecuteMultiple(group, componentCount, 
                    [&, shadow=shadow, lightEntity=lightEntity](JobData& data) {
                    auto component = &shadow->views[data.idx];
                    auto frustum = Volume::Frustum(component->frustumMatrix);

                    auto shadowPass = renderList.GetShadowPass(lightEntity, data.idx);
                    if (shadowPass == nullptr)
                        shadowPass = renderList.NewShadowPass(lightEntity, data.idx);

                    shadowPass->NewFrame(scene, meshes);
                    scene->GetRenderList(frustum, shadowPass);
                    shadowPass->Update(camera.GetLocation());
                    shadowPass->FillBuffers();
                    renderList.FinishPass(shadowPass);
                    });
            }

            JobSystem::Wait(group);

            auto mainPass = renderList.GetMainPass();
            if (mainPass == nullptr)
                mainPass = renderList.NewMainPass();

            mainPass->NewFrame(scene, meshes);
            scene->GetRenderList(camera.frustum, mainPass);
            mainPass->Update(camera.GetLocation());
            mainPass->FillBuffers();
            renderList.FinishPass(mainPass);

        }

        void MainRenderer::PreintegrateBRDF() {

            auto pipelineConfig = PipelineConfig("brdf/preintegrateDFG.csh");
            auto computePipeline = PipelineManager::GetPipeline(pipelineConfig);

            const int32_t res = 256;
            dfgPreintegrationTexture = Texture::Texture2D(res, res, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            auto commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue, true);

            commandList->BeginCommands();
            commandList->BindPipeline(computePipeline);

            auto barrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            commandList->ImageMemoryBarrier(barrier.Update(dfgPreintegrationTexture.image),
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            uint32_t groupCount = res / 8;

            commandList->BindImage(dfgPreintegrationTexture.image, 3, 0);
            commandList->Dispatch(groupCount, groupCount, 1);

            barrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);
            commandList->ImageMemoryBarrier(barrier.Update(dfgPreintegrationTexture.image),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            commandList->EndCommands();
            device->FlushCommandList(commandList);

        }

        PipelineConfig MainRenderer::GetPipelineConfigForPrimitives(Ref<Graphics::FrameBuffer> &frameBuffer,
            Buffer::VertexArray &vertexArray, VkPrimitiveTopology topology, bool testDepth) {

            const auto shaderConfig = ShaderConfig {
                { "primitive.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                { "primitive.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = frameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = topology;
            pipelineDesc.depthStencilInputInfo.depthTestEnable = testDepth;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            pipelineDesc.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }

    }

}
