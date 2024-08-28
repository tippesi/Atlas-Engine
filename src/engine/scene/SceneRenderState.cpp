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

            if (blasBuffers.size() < meshIdToBindlessIdx.size()) {
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

                // Not all meshes might have a bvh
                if (!mesh->IsBVHBuilt())
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

        auto bindlessTextureBuffersUpdate = [&](JobData&) {
            JobSystem::Wait(bindlessTextureMapUpdateJob);

            if (images.size() < textureToBindlessIdx.size()) {
                images.resize(textureToBindlessIdx.size());
            }

            for (const auto& [texture, idx] : textureToBindlessIdx) {

                images[idx] = texture->image;

            }
            };

        auto bindlessTextureMapUpdate = [&, bindlessTextureBuffersUpdate](JobData&) {
            auto meshes = scene->GetMeshes();
            textureToBindlessIdx.clear();

            std::set<Ref<Material>> materials;
            std::set<Ref<Texture::Texture2D>> textures;

            uint32_t textureIdx = 0;
            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded()) continue;

                for (auto& material : mesh->data.materials)
                    if (material.IsLoaded())
                        materials.insert(material.Get());
            }

            for (const auto& material : materials) {
                if (material->HasBaseColorMap())
                    textures.insert(material->baseColorMap.Get());
                if (material->HasOpacityMap())
                    textures.insert(material->opacityMap.Get());
                if (material->HasNormalMap())
                    textures.insert(material->normalMap.Get());
                if (material->HasRoughnessMap())
                    textures.insert(material->roughnessMap.Get());
                if (material->HasMetalnessMap())
                    textures.insert(material->metalnessMap.Get());
                if (material->HasAoMap())
                    textures.insert(material->aoMap.Get());
                if (material->HasDisplacementMap())
                    textures.insert(material->displacementMap.Get());
            }

            for (const auto& texture : textures) {

                textureToBindlessIdx[texture] = textureIdx++;

            }
            };

        JobSystem::Wait(bindlessTextureMapUpdateJob);
        JobSystem::Wait(prepareBindlessTexturesJob);

        JobSystem::Execute(bindlessTextureMapUpdateJob, bindlessTextureMapUpdate);
        JobSystem::Execute(prepareBindlessTexturesJob, bindlessTextureBuffersUpdate);

    }

    void SceneRenderState::UpdateOtherTextureBindlessData() {

        JobSystem::Wait(bindlessOtherTextureMapUpdateJob);

        JobSystem::Execute(bindlessOtherTextureMapUpdateJob, [&](JobData&) {
            auto lightSubset = scene->entityManager.GetSubset<LightComponent>();
            for (auto entity : lightSubset) {
                const auto& lightComponent = lightSubset.Get(entity);

                if (!lightComponent.shadow)
                    continue;

                if (lightComponent.shadow->useCubemap) {

                }
                else {

                }
            }
            });

    }

    void SceneRenderState::FillRenderList() {

        if (!scene->HasMainCamera())
            return;

        JobSystem::Wait(fillRenderListJob);

        JobSystem::Execute(fillRenderListJob, [&](JobData&) {
            auto& camera = scene->GetMainCamera();

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
                    [&, shadow = shadow, lightEntity = lightEntity](JobData& data) {
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

            });
    }

    void SceneRenderState::CullAndSortLights() {

        JobSystem::Wait(cullAndSortLightsJob);

        JobSystem::Execute(cullAndSortLightsJob, [&](JobData&) {
            auto& camera = scene->GetMainCamera();

            lightEntities.clear();
            auto lightSubset = scene->GetSubset<LightComponent>();
            for (auto& lightEntity : lightSubset) {
                auto& light = lightEntity.GetComponent<LightComponent>();
                lightEntities.emplace_back(LightEntity{ lightEntity, light, -1 });
            }

            if (lightEntities.size() > 1) {
                std::sort(lightEntities.begin(), lightEntities.end(),
                    [&](const LightEntity& light0, const LightEntity& light1) {
                        if (light0.comp.isMain)
                            return true;

                        if (light0.comp.type == LightType::DirectionalLight)
                            return true;

                        if (light0.comp.type == LightType::PointLight &&
                            light1.comp.type == LightType::PointLight) {
                            return glm::distance2(light0.comp.transformedProperties.point.position, camera.GetLocation())
                                < glm::distance2(light1.comp.transformedProperties.point.position, camera.GetLocation());
                        }

                        return false;
                    });
            }

            std::vector<Renderer::Light> lights;
            if (lightEntities.size()) {
                lights.reserve(lightEntities.size());
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
                        lightUniform.radius = prop.point.radius;
                        lightUniform.attenuation = prop.point.attenuation;
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
                        shadowUniform.mapIdx = entity.mapIdx;

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
                                    shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                                        cascade->viewMatrix * camera.invViewMatrix;
                                }
                                else if (light.type == LightType::PointLight) {
                                    if (i == 0)
                                        shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix;
                                    else
                                        shadowUniform.cascades[i].cascadeSpace = glm::translate(mat4(1.0f), -prop.point.position) * camera.invViewMatrix;
                                }
                                shadowUniform.cascades[i].texelSize = texelSize;
                            }
                            else {
                                auto cascade = &shadow->views[componentCount - 1];
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                            }
                        }
                    }

                    lights.emplace_back(lightUniform);
                }
            }
            else {
                // We need to have at least a fake light
                auto type = 0;
                auto packedType = reinterpret_cast<float&>(type);
                lights.emplace_back(Renderer::Light {
                        .color = vec4(vec3(0.0f), packedType),
                        .intensity = 0.0f,
                        .direction = vec4(0.0f, -1.0f, 0.0f, 0.0f)
                    });
            }

            if (lightBuffer.GetElementCount() < lightEntities.size()) {
                lightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::Light), lightEntities.size(), lights.data());
            }
            else {
                lightBuffer.SetData(lights.data(), 0, lights.size());
            }

            });

    }

    void SceneRenderState::WaitForAsyncWorkCompletion() {

        JobSystem::Wait(bindlessMeshMapUpdateJob);
        JobSystem::Wait(bindlessTextureMapUpdateJob);
        JobSystem::Wait(bindlessOtherTextureMapUpdateJob);
        JobSystem::Wait(materialUpdateJob);
        JobSystem::Wait(rayTracingWorldUpdateJob);
        JobSystem::Wait(prepareBindlessMeshesJob);
        JobSystem::Wait(prepareBindlessTexturesJob);
        JobSystem::Wait(fillRenderListJob);
        JobSystem::Wait(cullAndSortLightsJob);

    }

}
