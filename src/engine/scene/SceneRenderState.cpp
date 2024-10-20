#include "SceneRenderState.h"
#include "Scene.h"
#include "renderer/helper/CommonStructures.h"

#include "common/ColorConverter.h"
#include "common/Packing.h"

#include "common/ColorConverter.h"
#include <glm/gtx/norm.hpp>

// Move most of the things in the main renderer, like the bindless update or the materials to here
// Also move rendering related map updates from the scene to here
namespace Atlas::Scene {

    SceneRenderState::SceneRenderState(Scene* scene) : scene(scene) {

        materialBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
            Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit, sizeof(Renderer::PackedMaterial));

    }

    SceneRenderState::~SceneRenderState() {
        
        WaitForAsyncWorkCompletion();
        
    }

    void SceneRenderState::PrepareMaterials() {

        JobSystem::Wait(materialUpdateJob);

        JobSystem::Execute(materialUpdateJob, [&](JobData&) {
            auto sceneMaterials = scene->GetMaterials();

            // For debugging purpose
            if (scene->irradianceVolume && scene->irradianceVolume->debug) {
                const auto& internalVolume = scene->irradianceVolume->internal;
                sceneMaterials.push_back(internalVolume.probeDebugMaterial);
                sceneMaterials.push_back(internalVolume.probeDebugActiveMaterial);
                sceneMaterials.push_back(internalVolume.probeDebugInactiveMaterial);
                sceneMaterials.push_back(internalVolume.probeDebugOffsetMaterial);
            }

            uint16_t idx = 0;

            if (!materials.empty()) {
                materials.clear();
                materialMap.clear();
            }

            for (auto material : sceneMaterials) {
                // Might happen due to the scene giving back materials on a mesh basis
                if (materialMap.contains(material.get()))
                    continue;

                Renderer::PackedMaterial packed;

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(material->baseColor), 0.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor), 0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(material->transmissiveColor), 0.0f));

                packed.emissiveIntensityTiling = glm::packHalf2x16(vec2(material->emissiveIntensity, material->tiling));

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

                packed.features |= material->HasBaseColorMap() ? Renderer::MaterialFeatures::FEATURE_BASE_COLOR_MAP : 0;
                packed.features |= material->HasOpacityMap() ? Renderer::MaterialFeatures::FEATURE_OPACITY_MAP : 0;
                packed.features |= material->HasNormalMap() ? Renderer::MaterialFeatures::FEATURE_NORMAL_MAP : 0;
                packed.features |= material->HasRoughnessMap() ? Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP : 0;
                packed.features |= material->HasMetalnessMap() ? Renderer::MaterialFeatures::FEATURE_METALNESS_MAP : 0;
                packed.features |= material->HasAoMap() ? Renderer::MaterialFeatures::FEATURE_AO_MAP : 0;
                packed.features |= material->HasEmissiveMap() ? Renderer::MaterialFeatures::FEATURE_EMISSIVE_MAP : 0;
                packed.features |= glm::length(material->transmissiveColor) > 0.0f ? Renderer::MaterialFeatures::FEATURE_TRANSMISSION : 0;
                packed.features |= material->vertexColors ? Renderer::MaterialFeatures::FEATURE_VERTEX_COLORS : 0;

                materials.push_back(packed);

                materialMap[material.get()] = idx++;
            }

            auto meshes = scene->GetMeshes();

            for (auto mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                auto impostor = mesh->impostor;

                if (!impostor)
                    continue;

                Renderer::PackedMaterial packed;

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(1.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(impostor->transmissiveColor), 1.0f));

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

                packed.features |= Renderer::MaterialFeatures::FEATURE_BASE_COLOR_MAP |
                    Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP |
                    Renderer::MaterialFeatures::FEATURE_METALNESS_MAP |
                    Renderer::MaterialFeatures::FEATURE_AO_MAP;
                packed.features |= glm::length(impostor->transmissiveColor) > 0.0f ?
                    Renderer::MaterialFeatures::FEATURE_TRANSMISSION : 0;

                materials.push_back(packed);

                materialMap[impostor.get()] = idx++;
            }

            if (materials.size() > materialBuffer.GetElementCount()) {
                materialBuffer.SetSize(materials.size());
            }

            if (!materials.empty())
                materialBuffer.SetData(materials.data(), 0, materials.size());
            });

    }

    void SceneRenderState::UpdateMeshBindlessData() {

        auto bindlessMeshBuffersUpdate = [&](JobData&) {
            JobSystem::Wait(bindlessMeshMapUpdateJob);

            if (blasBuffers.size() != meshIdToBindlessIdx.size()) {
                blasBuffers.resize(meshIdToBindlessIdx.size());
                triangleBuffers.resize(meshIdToBindlessIdx.size());
                bvhTriangleBuffers.resize(meshIdToBindlessIdx.size());
                triangleOffsetBuffers.resize(meshIdToBindlessIdx.size());
            }

            for (const auto& [meshId, idx] : meshIdToBindlessIdx) {
                if (!scene->registeredMeshes.contains(meshId)) continue;

                const auto& mesh = scene->registeredMeshes[meshId].resource;

                auto blasBuffer = mesh->blasNodeBuffer.Get();
                auto triangleBuffer = mesh->triangleBuffer.Get();
                auto bvhTriangleBuffer = mesh->bvhTriangleBuffer.Get();
                auto triangleOffsetBuffer = mesh->triangleOffsetBuffer.Get();

                AE_ASSERT(triangleBuffer != nullptr);

                blasBuffers[idx] = blasBuffer;
                triangleBuffers[idx] = triangleBuffer;
                bvhTriangleBuffers[idx] = bvhTriangleBuffer;
                triangleOffsetBuffers[idx] = triangleOffsetBuffer;
            }
            };

        auto bindlessMeshMapUpdate = [&, bindlessMeshBuffersUpdate](JobData&) {
            auto meshes = scene->GetMeshes();

            meshIdToBindlessIdx.clear();

            uint32_t bufferIdx = 0;
            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded()) continue;

                // Not all meshes might have a bvh and not all blases will be built in frame, so skip them if they are not ready
                if (!mesh->IsBVHBuilt() || mesh->IsBVHBuilt() && mesh->blas && !mesh->blas->isDynamic && !mesh->blas->isBuilt)
                    continue;

                meshIdToBindlessIdx[mesh.GetID()] = bufferIdx++;
            }
            };

        JobSystem::Wait(bindlessMeshMapUpdateJob);
        JobSystem::Wait(prepareBindlessMeshesJob);

        JobSystem::Execute(bindlessMeshMapUpdateJob, bindlessMeshMapUpdate);
        JobSystem::Execute(prepareBindlessMeshesJob, bindlessMeshBuffersUpdate);

    }

    void SceneRenderState::UpdateTextureBindlessData() {

        auto bindlessTextureMapUpdate = [&](JobData&) {
            auto meshes = scene->GetMeshes();
            textureToBindlessIdx.clear();
            textures.clear();

            std::set<Ref<Material>> materialSet;
            std::set<Ref<Texture::Texture2D>> textureSet;

            uint32_t textureIdx = 0;
            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded()) continue;

                for (auto& material : mesh->data.materials)
                    if (material.IsLoaded())
                        materialSet.insert(material.Get());
            }

            for (const auto& material : materialSet) {
                if (material->HasBaseColorMap())
                    textureSet.insert(material->baseColorMap.Get());
                if (material->HasOpacityMap())
                    textureSet.insert(material->opacityMap.Get());
                if (material->HasNormalMap())
                    textureSet.insert(material->normalMap.Get());
                if (material->HasRoughnessMap())
                    textureSet.insert(material->roughnessMap.Get());
                if (material->HasMetalnessMap())
                    textureSet.insert(material->metalnessMap.Get());
                if (material->HasAoMap())
                    textureSet.insert(material->aoMap.Get());
                if (material->HasDisplacementMap())
                    textureSet.insert(material->displacementMap.Get());
                if (material->HasEmissiveMap())
                    textureSet.insert(material->emissiveMap.Get());
            }

            for (const auto& texture : textureSet) {

                textureToBindlessIdx[texture] = textureIdx++;
                textures.push_back(texture->image);

            }
            };

        JobSystem::Wait(bindlessTextureMapUpdateJob);

        JobSystem::Execute(bindlessTextureMapUpdateJob, bindlessTextureMapUpdate);

    }

    void SceneRenderState::UpdateOtherTextureBindlessData() {

        auto lightSubset = scene->GetSubset<LightComponent>();
        JobSystem::Wait(bindlessOtherTextureMapUpdateJob);

        JobSystem::Execute(bindlessOtherTextureMapUpdateJob, [&, lightSubset](JobData&) {

            cubemapToBindlessIdx.clear();
            textureArrayToBindlessIdx.clear();

            cubemaps.clear();
            textureArrays.clear();

            uint32_t textureArrayIdx = 0;
            uint32_t cubemapIdx = 0;
            for (auto entity : lightSubset) {
                const auto& lightComponent = entity.GetComponent<LightComponent>();

                if (!lightComponent.shadow)
                    continue;

                if (lightComponent.shadow->useCubemap) {
                    cubemapToBindlessIdx[lightComponent.shadow->cubemap] = cubemapIdx++;
                    cubemaps.push_back(lightComponent.shadow->cubemap->image);
                }
                else {
                    textureArrayToBindlessIdx[lightComponent.shadow->maps] = textureArrayIdx++;
                    textureArrays.push_back(lightComponent.shadow->maps->image);
                }
            }
            });

    }

    void SceneRenderState::FillRenderList() {

        if (!scene->HasMainCamera())
            return;

        JobSystem::Wait(fillRenderListJob);
        
        auto lightSubset = scene->GetSubset<LightComponent>();
        auto camera = scene->GetMainCamera();

        JobSystem::Execute(fillRenderListJob, [&, lightSubset, camera](JobData&) {           

            auto meshes = scene->GetMeshes();
            renderList.NewFrame(scene);

            JobGroup group{ JobPriority::High };
            for (auto& lightEntity : lightSubset) {

                auto& light = lightEntity.GetComponent<LightComponent>();
                if (!light.shadow || !light.shadow->update)
                    continue;

                auto& shadow = light.shadow;

                auto componentCount = shadow->longRange ?
                    shadow->viewCount - 1 : shadow->viewCount;

                JobSystem::ExecuteMultiple(group, componentCount,
                    [&, shadow = shadow, lightEntity = lightEntity](JobData& data) {
                        auto component = &shadow->views[data.idx];
                        auto frustum = Volume::Frustum(component->frustumMatrix);

                        auto shadowPass = renderList.GetShadowPass(lightEntity, data.idx);
                        if (shadowPass == nullptr)
                            shadowPass = renderList.NewShadowPass(lightEntity, data.idx);

                        shadowPass->NewFrame(scene, meshes, renderList.meshIdToMeshMap);
                        scene->GetRenderList(frustum, shadowPass);
                        shadowPass->Update(camera.GetLocation(), renderList.meshIdToMeshMap);
                        shadowPass->FillBuffers();
                        renderList.FinishPass(shadowPass);
                    });
            }

            JobSystem::Wait(group);

            auto mainPass = renderList.GetMainPass();
            if (mainPass == nullptr)
                mainPass = renderList.NewMainPass();

            mainPass->NewFrame(scene, meshes, renderList.meshIdToMeshMap);
            scene->GetRenderList(camera.frustum, mainPass);
            mainPass->Update(camera.GetLocation(), renderList.meshIdToMeshMap);
            mainPass->FillBuffers();
            renderList.FinishPass(mainPass);
            });
    }

    void SceneRenderState::CullAndSortLights() {

        JobSystem::Wait(cullAndSortLightsJob);

        JobSystem::Execute(cullAndSortLightsJob, [&](JobData&) {
            auto& camera = scene->GetMainCamera();

            lightEntities.clear();
            lightEntities.reserve(scene->GetComponentCount<LightComponent>());
            auto lightSubset = scene->GetSubset<LightComponent>();
            for (auto& lightEntity : lightSubset) {
                auto& light = lightEntity.GetComponent<LightComponent>();
                if (!light.IsVisible(camera.frustum))
                    continue;
                lightEntities.emplace_back(LightEntity{ lightEntity, light });
            }

            if (lightEntities.size() > 1) {
                auto cameraLocation = camera.GetLocation();
                auto getPositionForLight = [cameraLocation](const LightEntity& light) -> vec3 {
                    switch (light.comp.type) {
                    case LightType::PointLight: return light.comp.transformedProperties.point.position;
                    case LightType::SpotLight: return light.comp.transformedProperties.spot.position;
                    default: return cameraLocation;
                    }
                    };

                std::sort(lightEntities.begin(), lightEntities.end(),
                    [&](const LightEntity& light0, const LightEntity& light1) {
                        if (light0.comp.isMain)
                            return true;

                        if (light0.comp.type == LightType::DirectionalLight)
                            return true;

                        return glm::distance2(getPositionForLight(light0), cameraLocation)
                            < glm::distance2(getPositionForLight(light1), cameraLocation);
                    });
            }

            JobSystem::Wait(bindlessOtherTextureMapUpdateJob);

            if (lightEntities.size()) {
                lights.clear();
                volumetricLights.clear();
                volumetricShadows.clear();

                for (const auto& entity : lightEntities) {
                    auto& light = entity.comp;

                    auto type = static_cast<uint32_t>(light.type);
                    auto packedType = reinterpret_cast<float&>(type);
                    Renderer::Light lightUniform {
                        .color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), packedType),
                        .intensity = light.intensity,
                        .scatteringFactor = 1.0f,
                    };

                    const auto& prop = light.transformedProperties;
                    if (light.type == LightType::DirectionalLight) {
                        lightUniform.direction = camera.viewMatrix * vec4(prop.directional.direction, 0.0f);
                    }
                    else if (light.type == LightType::PointLight) {
                        lightUniform.location = camera.viewMatrix * vec4(prop.point.position, 1.0f);
                        lightUniform.direction.w = prop.point.radius;
                    }
                    else if (light.type == LightType::SpotLight) {
                        lightUniform.location = camera.viewMatrix * vec4(prop.spot.position, 1.0f);
                        lightUniform.direction = camera.viewMatrix * vec4(prop.spot.direction, 0.0f);
                        lightUniform.direction.w = prop.spot.radius;
                        
                        auto tanOuter = tanf(prop.spot.outerConeAngle);
                        auto cosOuter = cosf(prop.spot.outerConeAngle);
                        auto cosInner = cosf(prop.spot.innerConeAngle);
                        auto angleScale = 1.0f / std::max(0.001f, cosInner - cosOuter);
                        auto angleOffset = -cosOuter * angleScale;

                        lightUniform.typeSpecific0 = angleScale;
                        lightUniform.typeSpecific1 = angleOffset;

                        uint32_t coneTrig = glm::packHalf2x16(vec2(cosOuter, tanOuter));
                        lightUniform.location.w = reinterpret_cast<float&>(coneTrig);
                    }

                    if (light.shadow) {
                        auto shadow = light.shadow;
                        auto& shadowUniform = lightUniform.shadow;
                        shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                        shadowUniform.bias = shadow->bias;
                        shadowUniform.edgeSoftness = shadow->edgeSoftness;
                        shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                        shadowUniform.cascadeCount = shadow->viewCount;
                        shadowUniform.resolution = vec2(shadow->resolution);
                        shadowUniform.mapIdx = shadow->useCubemap ? cubemapToBindlessIdx[shadow->cubemap] : textureArrayToBindlessIdx[shadow->maps];

                        auto componentCount = shadow->viewCount;
                        for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                            if (i < componentCount) {
                                auto cascade = &shadow->views[i];
                                auto frustum = Volume::Frustum(cascade->frustumMatrix);
                                auto corners = frustum.GetCorners();
                                auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                                    abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                                if (light.type == LightType::DirectionalLight) {
                                    auto matrix = cascade->projectionMatrix *
                                        cascade->viewMatrix * camera.invViewMatrix;
                                    shadowUniform.cascades[i].cascadeSpace = glm::transpose(matrix);

                                    mat4 reTransposed = mat4(glm::transpose(shadowUniform.cascades[i].cascadeSpace));

                                    AE_ASSERT(reTransposed == matrix);
                                }
                                shadowUniform.cascades[i].texelSize = texelSize;
                            }
                            else {
                                auto cascade = &shadow->views[componentCount - 1];
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                            }
                        }

                        if (light.type == LightType::PointLight) {
                            auto projectionMatrix = shadow->views[0].projectionMatrix;
                            shadowUniform.cascades[0].cascadeSpace = glm::transpose(glm::translate(mat4(1.0f), -prop.point.position) * camera.invViewMatrix);
                            shadowUniform.cascades[1].cascadeSpace[0] = projectionMatrix[0];
                            shadowUniform.cascades[1].cascadeSpace[1] = projectionMatrix[1];
                            shadowUniform.cascades[1].cascadeSpace[2] = projectionMatrix[2];
                            shadowUniform.cascades[2].cascadeSpace[0] = projectionMatrix[3];
                        }
                        else if (light.type == LightType::SpotLight) {
                            auto cascadeMatrix = shadow->views[0].projectionMatrix *
                                shadow->views[0].viewMatrix * camera.invViewMatrix;
                            shadowUniform.cascades[0].cascadeSpace[0] = cascadeMatrix[0];
                            shadowUniform.cascades[0].cascadeSpace[1] = cascadeMatrix[1];
                            shadowUniform.cascades[0].cascadeSpace[2] = cascadeMatrix[2];
                            shadowUniform.cascades[1].cascadeSpace[0] = cascadeMatrix[3];
                        }
                    }
                    else {
                        lightUniform.shadow.mapIdx = -1;
                    }

                    lights.push_back(lightUniform);

                    if (light.volumetricIntensity > 0.0f) {
                        Renderer::VolumetricLight volumetricLightUniform{
                            .location = lightUniform.location,
                            .direction = lightUniform.direction,
                            .color = lightUniform.color,
                            .intensity = light.volumetricIntensity * light.intensity,
                            .typeSpecific0 = lightUniform.typeSpecific0,
                            .typeSpecific1 = lightUniform.typeSpecific1,
                            .shadowIdx = light.shadow ? int32_t(volumetricShadows.size()) : -1
                        };

                        volumetricLights.push_back(volumetricLightUniform);
                        volumetricShadows.push_back(lightUniform.shadow);
                    }
                }
            }
            else {
                // We need to have at least a fake light
                auto type = 0;
                auto packedType = reinterpret_cast<float&>(type);
                lights.emplace_back(Renderer::Light {
                        .direction = vec4(0.0f, -1.0f, 0.0f, 0.0f),
                        .color = vec4(vec3(0.0f), packedType),
                        .intensity = 0.0f,
                    });
            }

            if (lightBuffer.GetElementCount() < lights.size()) {
                lightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::Light), lights.size(), lights.data());
            }
            else {
                lightBuffer.SetData(lights.data(), 0, lights.size());
            }

            if (volumetricLights.empty()) {
                volumetricLights.emplace_back(Renderer::VolumetricLight {
                    .intensity = 0.0f,
                    .shadowIdx = -1,
                    });
                volumetricShadows.emplace_back(Renderer::Shadow {});
            }                

            if (volumetricLightBuffer.GetElementCount() < volumetricLights.size()) {
                volumetricLightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::VolumetricLight), volumetricLights.size(), volumetricLights.data());
            }
            else {
                volumetricLightBuffer.SetData(volumetricLights.data(), 0, volumetricLights.size());
            }

            if (volumetricShadowBuffer.GetElementCount() < volumetricShadows.size()) {
                volumetricShadowBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::Shadow), volumetricShadows.size(), volumetricShadows.data());
            }
            else {
                volumetricShadowBuffer.SetData(volumetricShadows.data(), 0, volumetricShadows.size());
            }

            });

    }

    void SceneRenderState::WaitForAsyncWorkCompletion() {

        // Assume scene work was done
        mainCameraSignal.Release();

        JobSystem::Wait(bindlessMeshMapUpdateJob);
        JobSystem::Wait(bindlessTextureMapUpdateJob);
        JobSystem::Wait(bindlessOtherTextureMapUpdateJob);
        JobSystem::Wait(materialUpdateJob);
        JobSystem::Wait(rayTracingWorldUpdateJob);
        JobSystem::Wait(prepareBindlessMeshesJob);
        JobSystem::Wait(fillRenderListJob);
        JobSystem::Wait(cullAndSortLightsJob);

        if (!renderList.wasCleared)
            renderList.Clear();

    }

}
