#include "RTData.h"
#include "Scene.h"

#include "../mesh/MeshData.h"
#include "../volume/BVH.h"
#include "../graphics/ASBuilder.h"

namespace Atlas {

    namespace Scene {

        RTData::RTData(Scene* scene) : scene(scene) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            hardwareRayTracing = device->support.hardwareRayTracing;

            triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUTriangle));
            bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(BVHTriangle));
            blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHNode));
            geometryTriangleOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));

            auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit |
                Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit;

            materialBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUMaterial));
            bvhInstanceBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHInstance));
            tlasNodeBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHNode));

        }

        void RTData::Build() {

            isValid = false;

            if (hardwareRayTracing) {
                BuildForHardwareRayTracing();
            }
            else {
                BuildForSoftwareRayTracing();
            }

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, true);

            isValid = true;

        }

        void RTData::Update(bool updateTriangleLights) {

            auto actors = scene->GetMeshActors();

            if (!actors.size()) return;

            UpdateMaterials();

            std::vector<GPUBVHInstance> gpuBvhInstances;
            std::vector<Volume::AABB> actorAABBs;

            for (auto& [_, meshInfo] : meshInfos) {
                meshInfo.instanceIndices.clear();
                meshInfo.matrices.clear();
            }

            for (auto& actor : actors) {
                if (!actor->mesh.IsLoaded())
                    continue;

                if (!meshInfos.contains(actor->mesh.GetID()))
                    continue;

                actorAABBs.push_back(actor->aabb);
                auto& meshInfo = meshInfos[actor->mesh.GetID()];

                auto inverseMatrix = mat3x4(glm::transpose(actor->inverseGlobalMatrix));

                GPUBVHInstance gpuBvhInstance = {
                    .inverseMatrix = inverseMatrix,
                    .blasOffset = meshInfo.offset,
                    .triangleOffset = meshInfo.triangleOffset
                };

                meshInfo.matrices.push_back(actor->globalMatrix);
                meshInfo.instanceIndices.push_back(uint32_t(gpuBvhInstances.size()));
                gpuBvhInstances.push_back(gpuBvhInstance);
            }

            if (!gpuBvhInstances.size())
                return;

            if (hardwareRayTracing) {
                UpdateForHardwareRayTracing(actors);
            }
            else {
                gpuBvhInstances = UpdateForSoftwareRayTracing(gpuBvhInstances, actorAABBs);
            }

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, false);

            if (updateTriangleLights)
                UpdateTriangleLights();

            bvhInstanceBuffer.SetSize(gpuBvhInstances.size());
            bvhInstanceBuffer.SetData(gpuBvhInstances.data(), 0, gpuBvhInstances.size());

        }

        void RTData::UpdateMaterials(bool updateTextures) {

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, updateTextures);

        }

        void RTData::UpdateMaterials(std::vector<GPUMaterial>& materials,
            bool updateTextures) {

            std::lock_guard lock(mutex);

            auto actors = scene->GetMeshActors();

            int32_t materialCount = 0;

            if (updateTextures)  UpdateTextures();

            auto meshes = scene->GetMeshes();
            materials.resize(materialAccess.size());

            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                for (auto& material : mesh->data.materials) {
                    if (!materialAccess.contains(&material))
                        continue;

                    auto materialIdx = materialAccess[&material];

                    GPUMaterial gpuMaterial;

                    gpuMaterial.baseColor = material.baseColor;
                    gpuMaterial.emissiveColor = material.emissiveColor;

                    gpuMaterial.opacity = material.opacity;

                    gpuMaterial.roughness = material.roughness;
                    gpuMaterial.metalness = material.metalness;
                    gpuMaterial.ao = material.ao;

                    gpuMaterial.reflectance = material.reflectance;

                    gpuMaterial.normalScale = material.normalScale;

                    gpuMaterial.invertUVs = mesh->invertUVs ? 1 : 0;
                    gpuMaterial.twoSided = material.twoSided ? 1 : 0;

                    if (material.HasBaseColorMap()) {
                        auto& slices = baseColorTextureAtlas.slices[material.baseColorMap.get()];

                        gpuMaterial.baseColorTexture = CreateGPUTextureStruct(slices);
                    }

                    if (material.HasOpacityMap()) {
                        auto& slices = opacityTextureAtlas.slices[material.opacityMap.get()];

                        gpuMaterial.opacityTexture = CreateGPUTextureStruct(slices);
                    }

                    if (material.HasNormalMap()) {
                        auto& slices = normalTextureAtlas.slices[material.normalMap.get()];

                        gpuMaterial.normalTexture = CreateGPUTextureStruct(slices);
                    }

                    if (material.HasRoughnessMap()) {
                        auto& slices = roughnessTextureAtlas.slices[material.roughnessMap.get()];

                        gpuMaterial.roughnessTexture = CreateGPUTextureStruct(slices);
                    }

                    if (material.HasMetalnessMap()) {
                        auto& slices = metalnessTextureAtlas.slices[material.metalnessMap.get()];

                        gpuMaterial.metalnessTexture = CreateGPUTextureStruct(slices);
                    }

                    if (material.HasAoMap()) {
                        auto& slices = aoTextureAtlas.slices[material.aoMap.get()];

                        gpuMaterial.aoTexture = CreateGPUTextureStruct(slices);
                    }

                    materials[materialIdx] = gpuMaterial;
                }
            }

            materialBuffer.SetSize(materials.size());
            materialBuffer.SetData(materials.data(), 0, materials.size());

        }

        void RTData::UpdateTextures() {

            auto meshes = scene->GetMeshes();

            std::vector<Ref<Texture::Texture2D>> baseColorTextures;
            std::vector<Ref<Texture::Texture2D>> opacityTextures;
            std::vector<Ref<Texture::Texture2D>> normalTextures;
            std::vector<Ref<Texture::Texture2D>> roughnessTextures;
            std::vector<Ref<Texture::Texture2D>> metalnessTextures;
            std::vector<Ref<Texture::Texture2D>> aoTextures;

            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                for (auto& material : mesh->data.materials) {
                    if (!materialAccess.contains(&material))
                        continue;

                    if (material.HasBaseColorMap())
                        baseColorTextures.push_back(material.baseColorMap);
                    if (material.HasOpacityMap())
                        opacityTextures.push_back(material.opacityMap);
                    if (material.HasNormalMap())
                        normalTextures.push_back(material.normalMap);
                    if (material.HasRoughnessMap())
                        roughnessTextures.push_back(material.roughnessMap);
                    if (material.HasMetalnessMap())
                        metalnessTextures.push_back(material.metalnessMap);
                    if (material.HasAoMap())
                        aoTextures.push_back(material.aoMap);
                }
            }

            const int32_t textureDownscale = 1;
            baseColorTextureAtlas = Texture::TextureAtlas(baseColorTextures, 1, textureDownscale);
            opacityTextureAtlas = Texture::TextureAtlas(opacityTextures, 1, textureDownscale);
            normalTextureAtlas = Texture::TextureAtlas(normalTextures, 1, textureDownscale);
            roughnessTextureAtlas = Texture::TextureAtlas(roughnessTextures, 1, textureDownscale);
            metalnessTextureAtlas = Texture::TextureAtlas(metalnessTextures, 1, textureDownscale);
            aoTextureAtlas = Texture::TextureAtlas(aoTextures, 1, textureDownscale);

        }

        void RTData::Clear() {

            isValid = false;

            baseColorTextureAtlas.Clear();
            opacityTextureAtlas.Clear();
            normalTextureAtlas.Clear();
            roughnessTextureAtlas.Clear();
            metalnessTextureAtlas.Clear();
            aoTextureAtlas.Clear();

        }

        bool RTData::IsValid() {

            return isValid;

        }

        GPUTexture RTData::CreateGPUTextureStruct(std::vector<Texture::TextureAtlas::Slice> slices) {

            GPUTexture texture;

            if (slices.size() > 0) texture.level0 = CreateGPUTextureLevelStruct(slices[0]);
            if (slices.size() > 1) texture.level1 = CreateGPUTextureLevelStruct(slices[1]);
            if (slices.size() > 2) texture.level2 = CreateGPUTextureLevelStruct(slices[2]);
            if (slices.size() > 3) texture.level3 = CreateGPUTextureLevelStruct(slices[3]);
            if (slices.size() > 4) texture.level4 = CreateGPUTextureLevelStruct(slices[4]);

            texture.valid = 1;

            return texture;

        }

        GPUTextureLevel RTData::CreateGPUTextureLevelStruct(Texture::TextureAtlas::Slice slice) {

            GPUTextureLevel level;

            level.x = slice.offset.x;
            level.y = slice.offset.y;

            level.width = slice.size.x;
            level.height = slice.size.y;

            level.layer = slice.layer;
            level.valid = 1;

            return level;

        }

        void RTData::BuildForSoftwareRayTracing() {

            std::vector<GPUTriangle> gpuTriangles;
            std::vector<BVHTriangle> gpuBvhTriangles;
            std::vector<GPUBVHNode> gpuBvhNodes;

            auto meshes = scene->GetMeshes();

            materialAccess.clear();

            int32_t materialCount = 0;
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                // Not all meshes might have a bvh
                if (!mesh->data.gpuTriangles.size())
                    continue;

                auto triangleOffset = int32_t(gpuTriangles.size());
                auto nodeOffset = int32_t(gpuBvhNodes.size());

                for (size_t i = 0; i < mesh->data.gpuBvhNodes.size(); i++) {
                    auto gpuBvhNode = mesh->data.gpuBvhNodes[i];

                    auto leftPtr = gpuBvhNode.leftPtr;
                    auto rightPtr = gpuBvhNode.rightPtr;

                    gpuBvhNode.leftPtr = leftPtr < 0 ? ~((~leftPtr) + triangleOffset) : leftPtr + nodeOffset;
                    gpuBvhNode.rightPtr = rightPtr < 0 ? ~((~rightPtr) + triangleOffset) : rightPtr + nodeOffset;

                    gpuBvhNodes.push_back(gpuBvhNode);
                }

                // Subtract and reassign material offset
                for (size_t i = 0; i < mesh->data.gpuTriangles.size(); i++) {
                    auto gpuTriangle = mesh->data.gpuTriangles[i];
                    auto gpuBvhTriangle = mesh->data.gpuBvhTriangles[i];

                    auto localMaterialIdx = reinterpret_cast<int32_t&>(gpuTriangle.d0.w);
                    auto materialIdx = localMaterialIdx + materialCount;

                    gpuTriangle.d0.w = reinterpret_cast<float&>(materialIdx);
                    gpuBvhTriangle.v1.w = reinterpret_cast<float&>(materialIdx);

                    gpuTriangles.push_back(gpuTriangle);
                    gpuBvhTriangles.push_back(gpuBvhTriangle);
                }

                for (auto& material : mesh->data.materials) {
                    materialAccess[&material] = materialCount++;
                }

                MeshInfo meshInfo = {
                    .offset = nodeOffset,
                    .triangleOffset = triangleOffset
                };
                meshInfos[mesh.GetID()] = meshInfo;

                BuildTriangleLightsForMesh(mesh);

            }

            if (!gpuTriangles.size())
                return;

            // Upload triangles
            triangleBuffer.SetSize(gpuTriangles.size());
            triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

            bvhTriangleBuffer.SetSize(gpuBvhTriangles.size());
            bvhTriangleBuffer.SetData(gpuBvhTriangles.data(), 0, gpuBvhTriangles.size());

            blasNodeBuffer.SetSize(gpuBvhNodes.size());
            blasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());

        }

        void RTData::BuildForHardwareRayTracing() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            std::vector<uint32_t> triangleOffsets;
            std::vector<GPUTriangle> gpuTriangles;

            Graphics::ASBuilder asBuilder;

            auto meshes = scene->GetMeshes();

            materialAccess.clear();
            blases.clear();

            int32_t materialCount = 0;
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                // Not all meshes might have a bvh
                if (!mesh->data.gpuTriangles.size())
                    continue;

                auto triangleOffset = int32_t(gpuTriangles.size());
                auto offset = int32_t(triangleOffsets.size());

                // Subtract and reassign material offset
                for (size_t i = 0; i < mesh->data.gpuTriangles.size(); i++) {
                    auto gpuTriangle = mesh->data.gpuTriangles[i];

                    auto localMaterialIdx = reinterpret_cast<int32_t&>(gpuTriangle.d0.w);
                    auto materialIdx = localMaterialIdx + materialCount;

                    gpuTriangle.d0.w = reinterpret_cast<float&>(materialIdx);

                    gpuTriangles.push_back(gpuTriangle);
                }

                for (auto& material : mesh->data.materials) {
                    materialAccess[&material] = materialCount++;
                }

                std::vector<Graphics::ASGeometryRegion> geometryRegions;
                for (auto& subData : mesh->data.subData) {
                    geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                        .indexCount = subData.indicesCount,
                        .indexOffset = subData.indicesOffset,
                        .opaque = !subData.material->HasOpacityMap() && subData.material->opacity == 1.0f
                        });
                }

                auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(mesh->vertexBuffer.buffer, mesh->indexBuffer.buffer,
                    mesh->vertexBuffer.elementCount, mesh->vertexBuffer.elementSize,
                    mesh->indexBuffer.elementSize, geometryRegions);

                blases.push_back(device->CreateBLAS(blasDesc));

                MeshInfo meshInfo = {
                    .blas = blases.back(),

                    .offset = offset,
                    .triangleOffset = triangleOffset
                };
                meshInfos[mesh.GetID()] = meshInfo;

                for (auto& subData : mesh->data.subData) {
                    auto totalTriangleOffset = triangleOffset + subData.indicesOffset / 3;
                    triangleOffsets.push_back(totalTriangleOffset);
                }

                BuildTriangleLightsForMesh(mesh);

            }

            if (!gpuTriangles.size())
                return;

            // Upload triangles
            triangleBuffer.SetSize(gpuTriangles.size());
            triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

            geometryTriangleOffsetBuffer.SetSize(triangleOffsets.size());
            geometryTriangleOffsetBuffer.SetData(triangleOffsets.data(), 0, triangleOffsets.size());

            asBuilder.BuildBLAS(blases);

        }

        std::vector<GPUBVHInstance> RTData::UpdateForSoftwareRayTracing(std::vector<GPUBVHInstance>& gpuBvhInstances,
            std::vector<Volume::AABB>& actorAABBs) {

            auto bvh = Volume::BVH(actorAABBs);

            auto& nodes = bvh.GetTree();
            auto gpuBvhNodes = std::vector<GPUBVHNode>(nodes.size());
            // Copy to GPU format
            for (size_t i = 0; i < nodes.size(); i++) {
                gpuBvhNodes[i].leftPtr = nodes[i].leftPtr;
                gpuBvhNodes[i].rightPtr = nodes[i].rightPtr;

                gpuBvhNodes[i].leftAABB.min = nodes[i].leftAABB.min;
                gpuBvhNodes[i].leftAABB.max = nodes[i].leftAABB.max;

                gpuBvhNodes[i].rightAABB.min = nodes[i].rightAABB.min;
                gpuBvhNodes[i].rightAABB.max = nodes[i].rightAABB.max;
            }

            // Order after the BVH build to fit the node indices
            std::vector<GPUBVHInstance> orderedGpuBvhInstances(bvh.refs.size());
            for (size_t i = 0; i < bvh.refs.size(); i++) {
                orderedGpuBvhInstances[i] = gpuBvhInstances[bvh.refs[i].idx];
            }

            tlasNodeBuffer.SetSize(gpuBvhNodes.size());
            tlasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());

            return orderedGpuBvhInstances;

        }

        void RTData::UpdateForHardwareRayTracing(std::vector<Actor::MeshActor*>& actors) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            Graphics::ASBuilder asBuilder;

            std::vector<VkAccelerationStructureInstanceKHR> instances;

            for (auto actor : actors) {
                if (!actor->mesh.IsLoaded())
                    continue;

                if (!meshInfos.contains(actor->mesh.GetID()))
                    continue;

                auto& meshInfo = meshInfos[actor->mesh.GetID()];

                VkAccelerationStructureInstanceKHR inst = {};
                VkTransformMatrixKHR transform;

                auto transposed = glm::transpose(actor->globalMatrix);
                std::memcpy(&transform, &transposed, sizeof(VkTransformMatrixKHR));

                inst.transform = transform;
                inst.instanceCustomIndex = meshInfo.offset;
                inst.accelerationStructureReference = meshInfo.blas->bufferDeviceAddress;
                inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                inst.mask = 0xFF;
                inst.instanceShaderBindingTableRecordOffset = 0;
                instances.push_back(inst);
            }

            auto tlasDesc = Graphics::TLASDesc();
            tlas = device->CreateTLAS(tlasDesc);

            asBuilder.BuildTLAS(tlas, instances);

        }

        void RTData::BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh> &mesh) {

            auto& gpuTriangles = mesh->data.gpuTriangles;
            auto& materials = mesh->data.materials;

            auto& meshInfo = meshInfos[mesh.GetID()];

            // Triangle lights
            for (size_t i = 0; i < gpuTriangles.size(); i++) {
                auto& triangle = gpuTriangles[i];
                auto idx = reinterpret_cast<int32_t&>(triangle.d0.w);
                auto& material = materials[idx];

                auto radiance = material.emissiveColor;
                auto brightness = dot(radiance, vec3(0.3333f));

                if (brightness > 0.0f) {
                    // Extract normal information again
                    auto cn0 = reinterpret_cast<int32_t&>(triangle.v0.w);
                    auto cn1 = reinterpret_cast<int32_t&>(triangle.v1.w);
                    auto cn2 = reinterpret_cast<int32_t&>(triangle.v2.w);

                    auto n0 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn0));
                    auto n1 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn1));
                    auto n2 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn2));

                    // Compute necessary information
                    auto P = (vec3(triangle.v0) + vec3(triangle.v1) + vec3(triangle.v2)) / 3.0f;
                    auto N = (n0 + n1 + n2) / 3.0f;

                    auto a = glm::distance(vec3(triangle.v1), vec3(triangle.v0));
                    auto b = glm::distance(vec3(triangle.v2), vec3(triangle.v0));
                    auto c = glm::distance(vec3(triangle.v1), vec3(triangle.v2));
                    auto p = 0.5f * (a + b + c);
                    auto area = glm::sqrt(p * (p - a) * (p - b) * (p - c));

                    auto weight = area * brightness;

                    uint32_t data = 0;
                    data |= (1 << 28u); // Type TRIANGLE_LIGHT (see RayTracingHelper.cpp)
                    data |= uint32_t(i);
                    auto cd = reinterpret_cast<float&>(data);

                    GPULight light;
                    light.P = vec4(P, 1.0f);
                    light.N = vec4(N, 0.0f);
                    light.color = vec4(radiance, 0.0f);
                    light.data = vec4(cd, weight, area, 0.0f);

                    meshInfo.triangleLights.push_back(light);
                }
            }

        }

        void RTData::UpdateTriangleLights() {

            triangleLights.clear();

            for (auto& [meshIdx, meshInfo] : meshInfos) {

                for (auto& light : meshInfo.triangleLights) {

                    for (size_t i = 0; i < meshInfo.instanceIndices.size(); i++) {

                        auto instanceIdx = meshInfo.instanceIndices[i];
                        auto& matrix = meshInfo.matrices[i];

                        vec3 P = matrix * vec4(vec3(light.P), 1.0f);
                        vec3 N = matrix * vec4(vec3(light.N), 0.0f);

                        auto transformedLight = light;

                        transformedLight.data.w = reinterpret_cast<float&>(instanceIdx);

                        transformedLight.P = vec4(P, light.P.w);
                        transformedLight.N = vec4(N, light.N.w);

                        triangleLights.push_back(transformedLight);

                    }

                }

            }

        }

    }

}