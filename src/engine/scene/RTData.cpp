#include "RTData.h"
#include "Scene.h"

#include "../volume/BVH.h"

namespace Atlas {

    namespace Scene {

        RTData::RTData(Scene* scene) : scene(scene) {

            triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUTriangle));
            bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(BVHTriangle));
            materialBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit, sizeof(GPUMaterial));
            nodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHNode));

        }

        void RTData::Update() {

            isValid = false;

            auto actors = scene->GetMeshActors();

            if (!actors.size()) return;

            int32_t indexCount = 0;
            int32_t vertexCount = 0;

            std::vector<GPUMaterial> materials;
            auto materialAccess = UpdateMaterials(materials, true);

            for (auto& actor : actors) {
                if (!actor->visible || !actor->mesh->data.IsLoaded())
                    continue;

                indexCount += actor->mesh->data->GetIndexCount();
                vertexCount += actor->mesh->data->GetVertexCount();
            }

            int32_t triangleCount = indexCount / 3;

            std::vector<vec3> vertices(indexCount);
            std::vector<vec3> normals(indexCount);
            std::vector<vec2> texCoords(indexCount);
            std::vector<int32_t> materialIndices(triangleCount);

            vertexCount = 0;

            for (auto& actor : actors) {
                if (!actor->visible || !actor->mesh->data.IsLoaded())
                    continue;

                auto& actorIndices = actor->mesh->data->indices.Get();
                auto& actorVertices = actor->mesh->data->vertices.Get();
                auto& actorNormals = actor->mesh->data->normals.Get();
                auto& actorTexCoords = actor->mesh->data->texCoords.Get();

                for (auto& subData : actor->mesh->data->subData) {

                    auto offset = subData.indicesOffset;
                    auto count = subData.indicesCount;
                    auto materialIndex = materialAccess[subData.material];

                    for (uint32_t i = 0; i < count; i++) {
                        auto j = actorIndices[i + offset];
                        vertices[vertexCount] = actorVertices[j];
                        normals[vertexCount] = vec3(actorNormals[j]);
                        if (actor->mesh->data->texCoords.ContainsData())
                            texCoords[vertexCount] = vec2(actorTexCoords[j]);
                        if ((vertexCount % 3) == 0) {
                            materialIndices[vertexCount / 3] = materialIndex;
                        }
                        vertexCount++;
                    }
                }

            }

            struct Triangle {
                vec3 v0;
                vec3 v1;
                vec3 v2;

                vec3 n0;
                vec3 n1;
                vec3 n2;

                vec2 uv0;
                vec2 uv1;
                vec2 uv2;

                int32_t materialIdx;
                bool endOfNode;
            };

            std::vector<Triangle> triangles(triangleCount);
            
            std::vector<Volume::AABB> aabbs(triangleCount);
            std::vector<Volume::BVHTriangle> bvhTriangles(triangleCount);

            triangleCount = 0;

            for (auto& actor : actors) {
                if (!actor->visible)
                    continue;

                auto actorTriangleCount = (int32_t)actor->mesh->data->GetIndexCount() / 3;

                auto matrix = actor->globalMatrix;

                for (int32_t i = 0; i < actorTriangleCount; i++) {

                    auto k = i + triangleCount;

                    // Transform everything
                    triangles[k].v0 = vec3(matrix * vec4(vertices[k * 3], 1.0f));
                    triangles[k].v1 = vec3(matrix * vec4(vertices[k * 3 + 1], 1.0f));
                    triangles[k].v2 = vec3(matrix * vec4(vertices[k * 3 + 2], 1.0f));

                    triangles[k].n0 = normalize(vec3(matrix * vec4(normals[k * 3], 0.0f)));
                    triangles[k].n1 = normalize(vec3(matrix * vec4(normals[k * 3 + 1], 0.0f)));
                    triangles[k].n2 = normalize(vec3(matrix * vec4(normals[k * 3 + 2], 0.0f)));

                    triangles[k].uv0 = texCoords[k * 3];
                    triangles[k].uv1 = texCoords[k * 3 + 1];
                    triangles[k].uv2 = texCoords[k * 3 + 2];

                    triangles[k].materialIdx = materialIndices[k];

                    auto min = glm::min(glm::min(triangles[k].v0,
                        triangles[k].v1), triangles[k].v2);
                    auto max = glm::max(glm::max(triangles[k].v0,
                        triangles[k].v1), triangles[k].v2);

                    bvhTriangles[k].v0 = triangles[k].v0;
                    bvhTriangles[k].v1 = triangles[k].v1;
                    bvhTriangles[k].v2 = triangles[k].v2;
                    bvhTriangles[k].idx = k;

                    aabbs[k] = Volume::AABB(min, max);

                }

                triangleCount += actorTriangleCount;

            }

            // Force memory to be cleared
            vertices.clear();
            normals.clear();
            texCoords.clear();
            materialIndices.clear();
            vertices.shrink_to_fit();
            normals.shrink_to_fit();
            texCoords.shrink_to_fit();
            materialIndices.shrink_to_fit();

            // Generate BVH
            auto bvh = Volume::BVH(aabbs, bvhTriangles);

            std::vector<GPUTriangle> gpuTriangles;
            std::vector<BVHTriangle> gpuBvhTriangles;

            for (auto& bvhTriangle : bvh.data) {

                auto& triangle = triangles[bvhTriangle.idx];

                auto v0v1 = triangle.v1 - triangle.v0;
                auto v0v2 = triangle.v2 - triangle.v0;

                auto uv0uv1 = triangle.uv1 - triangle.uv0;
                auto uv0uv2 = triangle.uv2 - triangle.uv0;

                auto r = 1.0f / (uv0uv1.x * uv0uv2.y - uv0uv2.x * uv0uv1.y);

                auto s = vec3(uv0uv2.y * v0v1.x - uv0uv1.y * v0v2.x,
                    uv0uv2.y * v0v1.y - uv0uv1.y * v0v2.y,
                    uv0uv2.y * v0v1.z - uv0uv1.y * v0v2.z) * r;

                auto t = vec3(uv0uv1.x * v0v2.x - uv0uv2.x * v0v1.x,
                    uv0uv1.x * v0v2.y - uv0uv2.x * v0v1.y,
                    uv0uv1.x * v0v2.z - uv0uv2.x * v0v1.z) * r;

                auto normal = glm::normalize(triangle.n0 + triangle.n1 + triangle.n2);

                auto tangent = glm::normalize(s - normal * dot(normal, s));
                auto handedness = (glm::dot(glm::cross(tangent, normal), t) < 0.0f ? 1.0f : -1.0f);

                auto bitangent = handedness * glm::normalize(glm::cross(tangent, normal));

                // Compress data
                auto pn0 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n0, 0.0f));
                auto pn1 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n1, 0.0f));
                auto pn2 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n2, 0.0f));

                auto pt = Common::Packing::PackSignedVector3x10_1x2(vec4(tangent, 0.0f));
                auto pbt = Common::Packing::PackSignedVector3x10_1x2(vec4(bitangent, 0.0f));

                auto puv0 = glm::packHalf2x16(triangle.uv0);
                auto puv1 = glm::packHalf2x16(triangle.uv1);
                auto puv2 = glm::packHalf2x16(triangle.uv2);

                auto cn0 = reinterpret_cast<float&>(pn0);
                auto cn1 = reinterpret_cast<float&>(pn1);
                auto cn2 = reinterpret_cast<float&>(pn2);

                auto ct = reinterpret_cast<float&>(pt);
                auto cbt = reinterpret_cast<float&>(pbt);

                auto cuv0 = reinterpret_cast<float&>(puv0);
                auto cuv1 = reinterpret_cast<float&>(puv1);
                auto cuv2 = reinterpret_cast<float&>(puv2);

                GPUTriangle gpuTriangle;

                gpuTriangle.v0 = vec4(triangle.v0, cn0);
                gpuTriangle.v1 = vec4(triangle.v1, cn1);
                gpuTriangle.v2 = vec4(triangle.v2, cn2);
                gpuTriangle.d0 = vec4(cuv0, cuv1, cuv2, reinterpret_cast<float&>(triangle.materialIdx));
                gpuTriangle.d1 = vec4(ct, cbt, bvhTriangle.endOfNode ? 1.0f : -1.0f, 0.0f);

                gpuTriangles.push_back(gpuTriangle);

                BVHTriangle gpuBvhTriangle;
                gpuBvhTriangle.v0 = vec4(triangle.v0, bvhTriangle.endOfNode ? 1.0f : -1.0f);
                gpuBvhTriangle.v1 = vec4(triangle.v1, reinterpret_cast<float&>(triangle.materialIdx));
                gpuBvhTriangle.v2 = vec4(triangle.v2, 0.0f);

                gpuBvhTriangles.push_back(gpuBvhTriangle);

            }

            triangles.clear();
            triangles.shrink_to_fit();

            // Free memory and upload data instantly
            bvh.data.clear();
            bvh.data.shrink_to_fit();

            // Upload triangles
            triangleBuffer.SetSize(gpuTriangles.size());
            triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

            bvhTriangleBuffer.SetSize(gpuBvhTriangles.size());
            bvhTriangleBuffer.SetData(gpuBvhTriangles.data(), 0, gpuBvhTriangles.size());

            auto& nodes = bvh.GetTree();
            auto gpuNodes = std::vector<GPUBVHNode>(nodes.size());
            // Copy to GPU format
            for (size_t i = 0; i < nodes.size(); i++) {
                gpuNodes[i].leftPtr = nodes[i].leftPtr;
                gpuNodes[i].rightPtr = nodes[i].rightPtr;

                gpuNodes[i].leftAABB.min = nodes[i].leftAABB.min;
                gpuNodes[i].leftAABB.max = nodes[i].leftAABB.max;

                gpuNodes[i].rightAABB.min = nodes[i].rightAABB.min;
                gpuNodes[i].rightAABB.max = nodes[i].rightAABB.max;
            }

            // Free original node memory
            nodes.clear();
            nodes.shrink_to_fit();

            // Upload nodes instantly
            nodeBuffer.SetSize(gpuNodes.size());
            nodeBuffer.SetData(gpuNodes.data(), 0, gpuNodes.size());

            triangleLights.clear();

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

                    // Compute neccessary information
                    auto P = (vec3(triangle.v0) + vec3(triangle.v1) + vec3(triangle.v2)) / 3.0f;
                    auto N = (n0 + n1 + n2) / 3.0f;

                    auto a = glm::distance(vec3(triangle.v1), vec3(triangle.v0));
                    auto b = glm::distance(vec3(triangle.v2), vec3(triangle.v0));
                    auto c = glm::distance(vec3(triangle.v1), vec3(triangle.v2));
                    auto p = 0.5f * (a + b + c);
                    auto area = glm::sqrt(p * (p - a) * (p - b) * (p - c));

                    auto weight = area * brightness;

                    // Compress light
                    auto pn = Common::Packing::PackSignedVector3x10_1x2(vec4(N, 0.0f));
                    auto cn = reinterpret_cast<float&>(pn);

                    uint32_t data = 0;
                    data |= (1 << 28u); // Type TRIANGLE_LIGHT (see RayTracingHelper.cpp)
                    data |= uint32_t(i);
                    auto cd = reinterpret_cast<float&>(data);

                    GPULight light;
                    light.data0 = vec4(P, radiance.r);
                    light.data1 = vec4(cd, weight, area, radiance.g);
                    light.N = vec4(N, radiance.b);

                    triangleLights.push_back(light);
                }
            }

            isValid = true;

        }

        void RTData::UpdateMaterials(bool updateTextures) {

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, updateTextures);

        }

        std::unordered_map<Material*, int32_t> RTData::UpdateMaterials(std::vector<GPUMaterial>& materials,
            bool updateTextures) {

            std::lock_guard lock(mutex);

            auto actors = scene->GetMeshActors();

            int32_t materialCount = 0;

            if (updateTextures)    UpdateTextures();

            std::unordered_set<Mesh::Mesh*> meshes;
            std::unordered_map<Material*, int32_t> materialAccess;

            // This is some awful stuff, there should be a way to just get the materials
            for (auto& actor : actors) {
                if (meshes.find(actor->mesh) == meshes.end() && actor->mesh->data.IsLoaded()) {
                    auto& actorMaterials = actor->mesh->data->materials;
                    for (auto& material : actorMaterials) {
                        materialAccess[&material] = materialCount;

                        GPUMaterial gpuMaterial;

                        gpuMaterial.baseColor = material.baseColor;
                        gpuMaterial.emissiveColor = material.emissiveColor;

                        gpuMaterial.opacity = material.opacity;

                        gpuMaterial.roughness = material.roughness;
                        gpuMaterial.metalness = material.metalness;
                        gpuMaterial.ao = material.ao;

                        gpuMaterial.reflectance = material.reflectance;

                        gpuMaterial.normalScale = material.normalScale;

                        gpuMaterial.invertUVs = actor->mesh->invertUVs ? 1 : 0;
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

                        materials.push_back(gpuMaterial);
                        materialCount++;
                    }
                    meshes.insert(actor->mesh);
                }
            }

            materialBuffer.SetSize(materials.size());
            materialBuffer.SetData(materials.data(), 0, materials.size());

            return materialAccess;

        }

        void RTData::UpdateTextures() {

            auto actors = scene->GetMeshActors();

            std::unordered_set<Mesh::Mesh*> meshes;
            std::vector<Ref<Texture::Texture2D>> baseColorTextures;
            std::vector<Ref<Texture::Texture2D>> opacityTextures;
            std::vector<Ref<Texture::Texture2D>> normalTextures;
            std::vector<Ref<Texture::Texture2D>> roughnessTextures;
            std::vector<Ref<Texture::Texture2D>> metalnessTextures;
            std::vector<Ref<Texture::Texture2D>> aoTextures;

            for (auto& actor : actors) {
                if (meshes.find(actor->mesh) == meshes.end() && actor->mesh->data.IsLoaded()) {
                    auto& actorMaterials = actor->mesh->data->materials;
                    for (auto& material : actorMaterials) {
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
                    meshes.insert(actor->mesh);
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

        RTData::GPUTexture RTData::CreateGPUTextureStruct(std::vector<Texture::TextureAtlas::Slice> slices) {

            GPUTexture texture;

            if (slices.size() > 0) texture.level0 = CreateGPUTextureLevelStruct(slices[0]);
            if (slices.size() > 1) texture.level1 = CreateGPUTextureLevelStruct(slices[1]);
            if (slices.size() > 2) texture.level2 = CreateGPUTextureLevelStruct(slices[2]);
            if (slices.size() > 3) texture.level3 = CreateGPUTextureLevelStruct(slices[3]);
            if (slices.size() > 4) texture.level4 = CreateGPUTextureLevelStruct(slices[4]);

            texture.valid = 1;

            return texture;

        }

        RTData::GPUTextureLevel RTData::CreateGPUTextureLevelStruct(Texture::TextureAtlas::Slice slice) {

            GPUTextureLevel level;

            level.x = slice.offset.x;
            level.y = slice.offset.y;

            level.width = slice.size.x;
            level.height = slice.size.y;

            level.layer = slice.layer;
            level.valid = 1;

            return level;

        }

    }

}