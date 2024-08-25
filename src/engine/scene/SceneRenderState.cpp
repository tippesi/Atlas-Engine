#include "SceneRenderState.h"
#include "Scene.h"

#include "common/ColorConverter.h"
#include "common/Packing.h"

// Move most of the things in the main renderer, like the bindless update or the materials to here
// Also move rendering related map updates from the scene to here
namespace Atlas::Scene {

    SceneRenderState::SceneRenderState(Scene* scene) : scene(scene) {

        materialBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | 
            Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit, sizeof(Renderer::PackedMaterial));

    }

    void SceneRenderState::PrepareMaterials() {

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
                packed.features |= material->HasOpacityMap() ?  Renderer::MaterialFeatures::FEATURE_OPACITY_MAP : 0;
                packed.features |= material->HasNormalMap() ?  Renderer::MaterialFeatures::FEATURE_NORMAL_MAP : 0;
                packed.features |= material->HasRoughnessMap() ?  Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP : 0;
                packed.features |= material->HasMetalnessMap() ?  Renderer::MaterialFeatures::FEATURE_METALNESS_MAP : 0;
                packed.features |= material->HasAoMap() ?  Renderer::MaterialFeatures::FEATURE_AO_MAP : 0;
                packed.features |= glm::length(material->transmissiveColor) > 0.0f ?  Renderer::MaterialFeatures::FEATURE_TRANSMISSION : 0;
                packed.features |= material->vertexColors ?  Renderer::MaterialFeatures::FEATURE_VERTEX_COLORS : 0;

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

                packed.features |=  Renderer::MaterialFeatures::FEATURE_BASE_COLOR_MAP | 
                    Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP | 
                    Renderer::MaterialFeatures::FEATURE_METALNESS_MAP | 
                    Renderer::MaterialFeatures::FEATURE_AO_MAP;
                packed.features |= glm::length(impostor->transmissiveColor) > 0.0f ? 
                    Renderer::MaterialFeatures::FEATURE_TRANSMISSION : 0;

                materials.push_back(packed);

                materialMap[impostor.get()] =  idx++;
            }

            if (materials.size() > materialBuffer.GetElementCount()) {
                materialBuffer.SetSize(materials.size());
            }

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

            JobSystem::Execute(prepareBindlessMeshesJob, bindlessMeshBuffersUpdate);
            };

        JobSystem::Execute(bindlessMeshMapUpdateJob, bindlessMeshMapUpdate);

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

            JobSystem::Execute(prepareBindlessTexturesJob, bindlessTextureBuffersUpdate);
        };
        
        JobSystem::Execute(bindlessTextureMapUpdateJob, bindlessTextureMapUpdate);

    }

    void SceneRenderState::UpdateOtherTextureBindlessData() {

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

}