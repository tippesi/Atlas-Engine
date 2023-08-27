#include "MainRenderer.h"
#include "helper/GeometryHelper.h"
#include "helper/HaltonSequence.h"

#include "../common/Packing.h"
#include "../Clock.h"

#define FEATURE_BASE_COLOR_MAP (1 << 1)
#define FEATURE_OPACITY_MAP (1 << 2)
#define FEATURE_NORMAL_MAP (1 << 3)
#define FEATURE_ROUGHNESS_MAP (1 << 4)
#define FEATURE_METALNESS_MAP (1 << 5)
#define FEATURE_AO_MAP (1 << 6)
#define FEATURE_TRANSMISSION (1 << 7)
#define FEATURE_VERTEX_COLORS (1 << 8)

namespace Atlas {

    namespace Renderer {

        void MainRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

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
            downscaleRenderer.Init(device);
            ddgiRenderer.Init(device);
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

            textRenderer.Init(device);
            textureRenderer.Init(device);

        }

        void MainRenderer::RenderScene(Viewport* viewport, RenderTarget* target, Camera* camera, 
            Scene::Scene* scene, Texture::Texture2D* texture, RenderBatch* batch) {

            auto commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue);

            commandList->BeginCommands();

            auto& taa = scene->postProcessing.taa;
            if (taa.enable) {
                auto jitter = 2.0f * haltonSequence[haltonIndex] - 1.0f;
                jitter.x /= (float)target->GetWidth();
                jitter.y /= (float)target->GetHeight();

                camera->Jitter(jitter * taa.jitterRange);
            }
            else {
                // Even if there is no TAA we need to update the jitter for other techniques
                // E.g. the reflections and ambient occlusion use reprojection
                camera->Jitter(vec2(0.0f));
            }

            Graphics::Profiler::BeginThread("Main renderer", commandList);
            Graphics::Profiler::BeginQuery("Render scene");

            FillRenderList(scene, camera);

            std::vector<PackedMaterial> materials;
            std::unordered_map<void*, uint16_t> materialMap;

            PrepareMaterials(scene, materials, materialMap);

            SetUniforms(scene, camera);

            commandList->BindImage(dfgPreintegrationTexture.image, dfgPreintegrationTexture.sampler, 0, 1);
            commandList->BindBuffer(globalUniformBuffer, 0, 0);

            auto materialBufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .hostAccess = Graphics::BufferHostAccess::Sequential,
                .data = materials.data(),
                .size = sizeof(PackedMaterial) * glm::max(materials.size(), size_t(1)),
            };
            auto materialBuffer = device->CreateBuffer(materialBufferDesc);
            commandList->BindBuffer(materialBuffer, 0, 2);

            if (scene->vegetation)
                vegetationRenderer.helper.PrepareInstanceBuffer(*scene->vegetation, camera, commandList);

            if (scene->sky.probe) {
                if (scene->sky.probe->update) {
                    FilterProbe(scene->sky.probe.get(), commandList);
                    //scene->sky.probe->update = false;
                }
            }
            else if (scene->sky.atmosphere) {
                atmosphereRenderer.Render(&scene->sky.atmosphere->probe, scene, commandList);
                FilterProbe(&scene->sky.atmosphere->probe, commandList);
            }

            // Bind before any shadows etc. are rendered, this is a shared buffer for all these passes
            commandList->BindBuffer(renderList.currentMatricesBuffer, 1, 0);
            commandList->BindBuffer(renderList.lastMatricesBuffer, 1, 1);
            commandList->BindBuffer(renderList.impostorMatricesBuffer, 1, 2);

            if (scene->irradianceVolume) {
                commandList->BindBuffer(ddgiUniformBuffer, 2, 26);
            }

            {
                shadowRenderer.Render(viewport, target, camera, scene, commandList, &renderList);

                terrainShadowRenderer.Render(viewport, target, camera, scene, commandList);
            }

            if (scene->sky.GetProbe()) {
                commandList->BindImage(scene->sky.GetProbe()->filteredSpecular.image,
                    scene->sky.GetProbe()->filteredSpecular.sampler, 1, 9);
                commandList->BindImage(scene->sky.GetProbe()->filteredDiffuse.image,
                    scene->sky.GetProbe()->filteredDiffuse.sampler, 1, 10);
            }

            volumetricCloudRenderer.RenderShadow(viewport, target, camera, scene, commandList);

            ddgiRenderer.TraceAndUpdateProbes(scene, commandList);

            {
                Graphics::Profiler::BeginQuery("Main render pass");

                commandList->BeginRenderPass(target->gBufferRenderPass, target->gBufferFrameBuffer, true);

                opaqueRenderer.Render(viewport, target, camera, scene, commandList, &renderList, materialMap);

                ddgiRenderer.DebugProbes(viewport, target, camera, scene, commandList, materialMap);

                vegetationRenderer.Render(viewport, target, camera, scene, commandList, materialMap);

                terrainRenderer.Render(viewport, target, camera, scene, commandList, materialMap);

                impostorRenderer.Render(viewport, target, camera, scene, commandList, &renderList, materialMap);

                commandList->EndRenderPass();

                Graphics::Profiler::EndQuery();
            }

            auto targetData = target->GetData(FULL_RES);

            commandList->BindImage(targetData->baseColorTexture->image, targetData->baseColorTexture->sampler, 1, 2);
            commandList->BindImage(targetData->normalTexture->image, targetData->normalTexture->sampler, 1, 3);
            commandList->BindImage(targetData->geometryNormalTexture->image, targetData->geometryNormalTexture->sampler, 1, 4);
            commandList->BindImage(targetData->roughnessMetallicAoTexture->image, targetData->roughnessMetallicAoTexture->sampler, 1, 5);
            commandList->BindImage(targetData->materialIdxTexture->image, targetData->materialIdxTexture->sampler, 1, 6);
            commandList->BindImage(targetData->depthTexture->image, targetData->depthTexture->sampler, 1, 7);

            if (!target->HasHistory()) {
                auto rtData = target->GetHistoryData(HALF_RES);
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers = {
                    {rtData->baseColorTexture->image, layout, access},
                    {rtData->depthTexture->image, layout, access},
                    {rtData->normalTexture->image, layout, access},
                    {rtData->geometryNormalTexture->image, layout, access},
                    {rtData->offsetTexture->image, layout, access},
                    {rtData->materialIdxTexture->image, layout, access},
                    {rtData->stencilTexture->image, layout, access},
                    {rtData->velocityTexture->image, layout, access},
                    {rtData->swapVelocityTexture->image, layout, access},
                    {target->historyAoTexture.image, layout, access},
                    {target->historyAoLengthTexture.image, layout, access},
                    {target->historyReflectionTexture.image, layout, access},
                    {target->historyReflectionMomentsTexture.image, layout, access},
                    {target->historyVolumetricCloudsTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            {
                if (scene->sky.probe) {
                    skyboxRenderer.Render(viewport, target, camera, scene, commandList);
                }
                else if (scene->sky.atmosphere) {
                    atmosphereRenderer.Render(viewport, target, camera, scene, commandList);
                }
            }

            downscaleRenderer.Downscale(target, commandList);

            aoRenderer.Render(viewport, target, camera, scene, commandList);

            rtrRenderer.Render(viewport, target, camera, scene, commandList);

            sssRenderer.Render(viewport, target, camera, scene, commandList);

            {
                Graphics::Profiler::BeginQuery("Lighting pass");

                commandList->ImageMemoryBarrier(target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                directLightRenderer.Render(viewport, target, camera, scene, commandList);

                commandList->ImageMemoryBarrier(target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                indirectLightRenderer.Render(viewport, target, camera, scene, commandList);

                Graphics::ImageBarrier outBarrier(target->lightingTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                commandList->ImageMemoryBarrier(outBarrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                Graphics::Profiler::EndQuery();
            }

            oceanRenderer.Render(viewport, target, camera, scene, commandList);

            downscaleRenderer.Downscale(target, commandList);

            {
                volumetricCloudRenderer.Render(viewport, target, camera, scene, commandList);

                volumetricRenderer.Render(viewport, target, camera, scene, commandList);
            }

            {
                taaRenderer.Render(viewport, target, camera, scene, commandList);

                target->Swap();

                postProcessRenderer.Render(viewport, target, camera, scene, commandList);
            }

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndThread();

            commandList->EndCommands();
            device->SubmitCommandList(commandList);

        }

        void MainRenderer::PathTraceScene(Viewport *viewport, PathTracerRenderTarget *target, Camera *camera,
            Scene::Scene *scene, Texture::Texture2D *texture) {

            auto commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue);

            commandList->BeginCommands();

            Graphics::Profiler::BeginThread("Path tracing", commandList);
            Graphics::Profiler::BeginQuery("Buffer operations");

            commandList->BindImage(dfgPreintegrationTexture.image, dfgPreintegrationTexture.sampler, 0, 1);

            auto globalUniforms = GlobalUniforms {
                .vMatrix = camera->viewMatrix,
                .pMatrix = camera->projectionMatrix,
                .ivMatrix = camera->invViewMatrix,
                .ipMatrix = camera->invProjectionMatrix,
                .pvMatrixLast = camera->GetLastJitteredMatrix(),
                .pvMatrixCurrent = camera->projectionMatrix * camera->viewMatrix,
                .jitterLast = camera->GetLastJitter(),
                .jitterCurrent = camera->GetJitter(),
                .cameraLocation = vec4(camera->location, 0.0f),
                .cameraDirection = vec4(camera->direction, 0.0f),
                .cameraUp = vec4(camera->up, 0.0f),
                .cameraRight = vec4(camera->right, 0.0f),
                .time = Clock::Get(),
                .deltaTime = Clock::GetDelta()
            };

            pathTraceGlobalUniformBuffer->SetData(&globalUniforms, 0, sizeof(GlobalUniforms));
            commandList->BindBuffer(pathTraceGlobalUniformBuffer, 0, 0);

            Graphics::Profiler::EndQuery();

            pathTracingRenderer.Render(viewport, target, ivec2(1, 1), camera, scene, commandList);

            Graphics::Profiler::BeginQuery("Post processing");

            commandList->BeginRenderPass(device->swapChain, true);
            
            textureRenderer.RenderTexture2D(commandList, viewport, &target->texture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0f, 1.0f, false, true);

            commandList->EndRenderPass();

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndThread();

            commandList->EndCommands();

            device->SubmitCommandList(commandList);

        }

        void MainRenderer::RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
            bool alphaBlending) {

            /*
            float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
            float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

            if (x > viewportWidth || y > viewportHeight ||
                y + height < 0 || x + width < 0) {
                return;
            }

            vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
            vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

            RenderRectangle(viewport, color, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);
            */

        }

        void MainRenderer::RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
            vec4 clipArea, vec4 blendArea, bool alphaBlending) {

            /*
            float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
            float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

            if (x > viewportWidth || y > viewportHeight ||
                y + height < 0 || x + width < 0) {
                return;
            }

            vertexArray.Bind();

            rectangleShader.Bind();

            glDisable(GL_CULL_FACE);

            if (framebuffer) {
                framebuffer->Bind(true);
            }
            else {
                glViewport(0, 0, viewport->width, viewport->height);
            }

            if (alphaBlending) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            rectangleProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
            rectangleOffset->SetValue(vec2(x, y));
            rectangleScale->SetValue(vec2(width, height));
            rectangleColor->SetValue(color);
            rectangleBlendArea->SetValue(blendArea);
            rectangleClipArea->SetValue(clipArea);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            if (alphaBlending) {
                glDisable(GL_BLEND);
            }

            if (framebuffer) {
                framebuffer->Unbind();
            }

            glEnable(GL_CULL_FACE);
            */

        }

        void MainRenderer::RenderBatched(Viewport* viewport, Camera* camera, RenderBatch* batch) {

            /*
            batch->TransferData();

            if (viewport)
                glViewport(viewport->x, viewport->y,
                    viewport->width, viewport->height);

            lineShader.Bind();

            lineViewMatrix->SetValue(camera->viewMatrix);
            lineProjectionMatrix->SetValue(camera->projectionMatrix);

            if (batch->GetLineCount()) {

                glLineWidth(batch->GetLineWidth());

                batch->BindLineBuffer();

                glDrawArrays(GL_LINES, 0, (GLsizei)batch->GetLineCount() * 2);

                glLineWidth(1.0f);

            }

            if (batch->GetTriangleCount()) {
                batch->BindTriangleBuffer();

                glDrawArrays(GL_TRIANGLES, 0, GLsizei(batch->GetTriangleCount() * 3));
            }
            */

        }

        void MainRenderer::RenderProbe(Lighting::EnvironmentProbe* probe, RenderTarget* target, Scene::Scene* scene) {

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

        void MainRenderer::FilterProbe(Lighting::EnvironmentProbe* probe, Graphics::CommandList* commandList) {

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

            commandList->BindImage(probe->filteredDiffuse.image, 3, 0);
            commandList->BindImage(probe->cubemap.image, probe->cubemap.sampler, 3, 1);

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

            struct PushConstants {
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
                    .cubeMapMipLevels = int32_t(probe->cubemap.image->mipLevels),
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

        void MainRenderer::SetUniforms(Scene::Scene *scene, Camera *camera) {

            auto globalUniforms = GlobalUniforms {
                .vMatrix = camera->viewMatrix,
                .pMatrix = camera->projectionMatrix,
                .ivMatrix = camera->invViewMatrix,
                .ipMatrix = camera->invProjectionMatrix,
                .pvMatrixLast = camera->GetLastJitteredMatrix(),
                .pvMatrixCurrent = camera->projectionMatrix * camera->viewMatrix,
                .jitterLast = camera->GetLastJitter(),
                .jitterCurrent = camera->GetJitter(),
                .cameraLocation = vec4(camera->location, 0.0f),
                .cameraDirection = vec4(camera->direction, 0.0f),
                .cameraUp = vec4(camera->up, 0.0f),
                .cameraRight = vec4(camera->right, 0.0f),
                .time = Clock::Get(),
                .deltaTime = Clock::GetDelta(),
                .frameCount = frameCount
            };

            auto frustumPlanes = camera->frustum.GetPlanes();
            std::copy(frustumPlanes.begin(), frustumPlanes.end(), &globalUniforms.frustumPlanes[0]);

            globalUniformBuffer->SetData(&globalUniforms, 0, sizeof(GlobalUniforms));

            if (scene->irradianceVolume) {
                auto volume = scene->irradianceVolume;
                auto ddgiUniforms = DDGIUniforms {
                    .volumeMin = vec4(volume->aabb.min, 1.0f),
                    .volumeMax = vec4(volume->aabb.max, 1.0f),
                    .volumeProbeCount = ivec4(volume->probeCount, 0),
                    .cellSize = vec4(volume->cellSize, 0.0f),
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
                ddgiUniformBuffer->SetData(&ddgiUniforms, 0, sizeof(DDGIUniforms));
            }
            else {
                auto ddgiUniforms = DDGIUniforms {
                    .volumeMin = vec4(0.0f),
                    .volumeMax = vec4(0.0f),
                    .volumeEnabled = 0
                };
                ddgiUniformBuffer->SetData(&ddgiUniforms, 0, sizeof(DDGIUniforms));
            }

            auto meshes = scene->GetMeshes();

            for (auto& mesh : meshes) {
                if (!mesh->impostor) continue;

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

        void MainRenderer::PrepareMaterials(Scene::Scene* scene, std::vector<PackedMaterial>& materials,
            std::unordered_map<void*, uint16_t>& materialMap) {

            auto sceneMaterials = scene->GetMaterials();

            // For debugging purpose
            if (scene->irradianceVolume && scene->irradianceVolume->debug) {
                sceneMaterials.push_back(&ddgiRenderer.probeDebugMaterial);
                sceneMaterials.push_back(&ddgiRenderer.probeDebugActiveMaterial);
                sceneMaterials.push_back(&ddgiRenderer.probeDebugInactiveMaterial);
                sceneMaterials.push_back(&ddgiRenderer.probeDebugOffsetMaterial);
            }

            uint16_t idx = 0;

            for (auto material : sceneMaterials) {
                PackedMaterial packed;

                auto emissiveIntensity = glm::max(glm::max(material->emissiveColor.r,
                    material->emissiveColor.g), material->emissiveColor.b);

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(material->baseColor, 0.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(material->emissiveColor / emissiveIntensity, 0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(material->transmissiveColor, 0.0f));

                packed.emissiveIntensityTiling = glm::packHalf2x16(vec2(emissiveIntensity, material->tiling));

                vec4 data0, data1, data2;

                data0.x = material->opacity;
                data0.y = material->roughness;
                data0.z = material->metalness;

                data1.x = material->ao;
                data1.y = material->HasNormalMap() ? material->normalScale : 0.0f;
                data1.z = material->HasDisplacementMap() ? material->displacementScale : 0.0f;

                data2.x = material->reflectance;
                // Note used
                data2.y = 0.0f;
                data2.z = 0.0f;

                packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
                packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
                packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

                packed.features = 0;

                packed.features |= material->HasBaseColorMap() ? FEATURE_BASE_COLOR_MAP : 0;
                packed.features |= material->HasOpacityMap() ? FEATURE_OPACITY_MAP : 0;
                packed.features |= material->HasNormalMap() ? FEATURE_NORMAL_MAP : 0;
                packed.features |= material->HasRoughnessMap() ? FEATURE_ROUGHNESS_MAP : 0;
                packed.features |= material->HasMetalnessMap() ? FEATURE_METALNESS_MAP : 0;
                packed.features |= material->HasAoMap() ? FEATURE_AO_MAP : 0;
                packed.features |= glm::length(material->transmissiveColor) > 0.0f ? FEATURE_TRANSMISSION : 0;
                packed.features |= material->vertexColors ? FEATURE_VERTEX_COLORS : 0;

                materials.push_back(packed);

                materialMap[material] = idx++;
            }
            
            auto meshes = scene->GetMeshes();

            for (auto mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                auto impostor = mesh->impostor;

                if (!impostor)
                    continue;

                PackedMaterial packed;

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(1.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(impostor->transmissiveColor, 1.0f));

                vec4 data0, data1, data2;

                data0.x = 1.0f;
                data0.y = 1.0f;
                data0.z = 1.0f;

                data1.x = 1.0f;
                data1.y = 0.0f;
                data1.z = 0.0f;

                data2.x = 0.5f;
                // Note used
                data2.y = 0.0f;
                data2.z = 0.0f;

                packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
                packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
                packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

                packed.features = 0;

                packed.features |= FEATURE_BASE_COLOR_MAP | 
                    FEATURE_ROUGHNESS_MAP | FEATURE_METALNESS_MAP | FEATURE_AO_MAP;
                packed.features |= glm::length(impostor->transmissiveColor) > 0.0f ? FEATURE_TRANSMISSION : 0;

                materials.push_back(packed);

                materialMap[impostor.get()] =  idx++;
            }


        }

        void MainRenderer::FillRenderList(Scene::Scene *scene, Atlas::Camera *camera) {

            renderList.NewFrame();
            renderList.NewMainPass();

            scene->GetRenderList(camera->frustum, renderList);
            renderList.Update(camera);

            auto lights = scene->GetLights();

            if (scene->sky.sun) {
                lights.push_back(scene->sky.sun.get());
            }

            for (auto light : lights) {
                if (!light->GetShadow())
                    continue;
                if (!light->GetShadow()->update)
                    continue;

                auto componentCount = light->GetShadow()->longRange ?
                    light->GetShadow()->componentCount - 1 :
                    light->GetShadow()->componentCount;

                for (int32_t i = 0; i < componentCount; i++) {
                    auto component = &light->GetShadow()->components[i];
                    auto frustum = Volume::Frustum(component->frustumMatrix);

                    renderList.NewShadowPass(light, i);
                    scene->GetRenderList(frustum, renderList);
                    renderList.Update(camera);
                }

            }

            renderList.FillBuffers();

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

            commandList->BindImage(dfgPreintegrationTexture.image, 0, 0);
            commandList->Dispatch(groupCount, groupCount, 1);

            barrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);
            commandList->ImageMemoryBarrier(barrier.Update(dfgPreintegrationTexture.image),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            commandList->EndCommands();
            device->FlushCommandList(commandList);

        }

    }

}
