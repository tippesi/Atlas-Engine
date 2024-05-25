#include "IrradianceVolume.h"

namespace Atlas {

    namespace Lighting {

        IrradianceVolume::IrradianceVolume(Volume::AABB aabb, ivec3 probeCount, int32_t cascadeCount,
            bool lowerResMoments) : probeCount(probeCount), cascadeCount(std::min(cascadeCount, MAX_IRRADIANCE_VOLUME_CASCADES)), 
            momRes(lowerResMoments ? 6 : 14), lowerResMoments(lowerResMoments) {

            auto irrRes = ivec2(this->irrRes + 2);
            irrRes.x *= probeCount.x;
            irrRes.y *= probeCount.z;

            auto momRes = ivec2(this->momRes + 2);
            momRes.x *= probeCount.x;
            momRes.y *= probeCount.z;

            SetAABB(aabb);

            internal = InternalIrradianceVolume(irrRes, momRes, probeCount, cascadeCount);
            internal.SetRayCount(rayCount, rayCountInactive);
               
        }

        ivec3 IrradianceVolume::GetIrradianceArrayOffset(ivec3 probeIndex, int32_t cascadeIndex) {

            auto irrRes = ivec2(this->irrRes + 2);

            return ivec3(probeIndex.x * irrRes.x + 1,
                probeIndex.z * irrRes.y + 1, probeIndex.y + cascadeIndex * probeCount.y);

        }

        ivec3 IrradianceVolume::GetMomentsArrayOffset(ivec3 probeIndex, int32_t cascadeIndex) {

            auto momRes = ivec2(this->momRes + 2);

            return ivec3(probeIndex.x * momRes.x + 1,
                probeIndex.z * momRes.y + 1, probeIndex.y);

        }

        vec3 IrradianceVolume::GetProbeLocation(ivec3 probeIndex, int32_t cascadeIndex) {

            return cascades[cascadeIndex].aabb.min + vec3(probeIndex) * cascades[cascadeIndex].cellSize;

        }

        void IrradianceVolume::SetAABB(Volume::AABB aabb) {

            this->aabb = aabb;

            auto center = (aabb.max + aabb.min) * 0.5f;

            for (int32_t i = 0; i < cascadeCount; i++) {
                cascades[i].size = aabb.max - aabb.min;
                cascades[i].cellSize = cascades[i].size / vec3(probeCount - ivec3(1));
                
                if (!scroll) {
                    // In this case we can just set the AABB
                    cascades[i].aabb = aabb;
                }
                else {
                    // In case of scrolling, just take discrete steps
                    ivec3 cascadeOffset = ivec3(center / cascades[i].cellSize);

                    cascades[i].offsetDifferences = cascadeOffset - cascades[i].offsets;
                    cascades[i].offsets = cascadeOffset;

                    vec3 offset = vec3(cascadeOffset) * cascades[i].cellSize;
                    cascades[i].aabb = Volume::AABB(-cascades[i].size / 2.0f + offset, cascades[i].size / 2.0f + offset);
                }

                aabb.min = center - cascades[i].size / (2.0f * splitCorrection);
                aabb.max = center + cascades[i].size / (2.0f * splitCorrection);
            }

        }

        void IrradianceVolume::SetRayCount(uint32_t rayCount, uint32_t rayCountInactive) {

            this->rayCount = rayCount;
            this->rayCountInactive = rayCountInactive;
            
            internal.SetRayCount(rayCount, rayCountInactive);
        
        }

        void IrradianceVolume::SetProbeCount(ivec3 probeCount, int32_t cascadeCount) {

            this->probeCount = probeCount;
            this->cascadeCount = std::min(cascadeCount, MAX_IRRADIANCE_VOLUME_CASCADES);

            auto irrRes = ivec2(this->irrRes + 2);
            irrRes.x *= probeCount.x;
            irrRes.y *= probeCount.z;

            auto momRes = ivec2(this->momRes + 2);
            momRes.x *= probeCount.x;
            momRes.y *= probeCount.z;

            internal = InternalIrradianceVolume(irrRes, momRes, probeCount, cascadeCount);
            internal.SetRayCount(rayCount, rayCountInactive);

        }

        void IrradianceVolume::ClearProbes() {

            auto irrRes = ivec2(this->irrRes + 2);
            irrRes.x *= probeCount.x;
            irrRes.y *= probeCount.z;

            auto momRes = ivec2(this->momRes + 2);
            momRes.x *= probeCount.x;
            momRes.y *= probeCount.z;

            internal.ClearProbes(irrRes, momRes, probeCount, cascadeCount);

        }

        void IrradianceVolume::ResetProbeOffsets() {

            internal.ResetProbeOffsets();

        }

        InternalIrradianceVolume::InternalIrradianceVolume(ivec2 irrRes, ivec2 momRes, ivec3 probeCount, int32_t cascadeCount) {

            rayDirBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4));
            rayDirInactiveBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4));
            probeStateBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4),
                probeCount.x * probeCount.y * probeCount.z * cascadeCount);
            historyProbeStateBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4),
                probeCount.x * probeCount.y * probeCount.z * cascadeCount);
            probeOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4),
                probeCount.x * probeCount.y * probeCount.z * cascadeCount);
            historyProbeOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4),
                probeCount.x * probeCount.y * probeCount.z * cascadeCount);

            irradianceArray0 = Texture::Texture2DArray(irrRes.x, irrRes.y, probeCount.y * cascadeCount,
                VK_FORMAT_A2B10G10R10_UNORM_PACK32, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            momentsArray0 = Texture::Texture2DArray(momRes.x, momRes.y, probeCount.y * cascadeCount,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            irradianceArray1 = Texture::Texture2DArray(irrRes.x, irrRes.y, probeCount.y * cascadeCount,
                VK_FORMAT_A2B10G10R10_UNORM_PACK32, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            momentsArray1 = Texture::Texture2DArray(momRes.x, momRes.y, probeCount.y * cascadeCount, 
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

            SwapTextures();
            ClearProbes(irrRes, momRes, probeCount, cascadeCount);

        }

        void InternalIrradianceVolume::SetRayCount(uint32_t rayCount, uint32_t rayCountInactive) {

            rayDirBuffer.SetSize(size_t(rayCount));
            rayDirInactiveBuffer.SetSize(size_t(rayCountInactive));
            FillRayBuffers();

        }

        std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>
            InternalIrradianceVolume::GetCurrentProbes() const {
        
            if (swapIdx == 0) {
                return std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>(
                    irradianceArray0, momentsArray0
                    );
            }
            else {
                return std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>(
                    irradianceArray1, momentsArray1
                    );
            }
        
        }

        std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>
            InternalIrradianceVolume::GetLastProbes() const {

            if (swapIdx == 0) {
                return std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>(
                    irradianceArray1, momentsArray1
                    );
            }
            else {
                return std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>(
                    irradianceArray0, momentsArray0
                    );
            }    

        }

        std::tuple<const Buffer::Buffer&, const Buffer::Buffer&>
            InternalIrradianceVolume::GetCurrentProbeBuffers() const {

            if (swapIdx == 0) {
                return std::tuple<const Buffer::Buffer&, const Buffer::Buffer&>(
                    probeStateBuffer, probeOffsetBuffer
                );
            }
            else {
                return std::tuple<const Buffer::Buffer&, const Buffer::Buffer&>(
                    historyProbeStateBuffer, historyProbeOffsetBuffer
                );
            }

        }

        std::tuple<const Buffer::Buffer&, const Buffer::Buffer&>
            InternalIrradianceVolume::GetLastProbeBuffers() const {

            if (swapIdx == 0) {
                return std::tuple<const Buffer::Buffer&, const Buffer::Buffer&>(
                    historyProbeStateBuffer, historyProbeOffsetBuffer
                );
            }
            else {
                return std::tuple<const Buffer::Buffer&, const Buffer::Buffer&>(
                    probeStateBuffer, probeOffsetBuffer
                );
            }

        }

        void InternalIrradianceVolume::SwapTextures() {

            swapIdx = (swapIdx + 1) % 2;

        }

        void InternalIrradianceVolume::FillRayBuffers() {
            // Low discrepency sequence for rays
            auto fibonacciSphere = [](float i, float n) -> vec3 {
                const float PHI = 1.6180339887498948482045868343656f;

                float phi = 2.0f * glm::pi<float>() * i * PHI - floorf(i * PHI);
                float cosTheta = 1.0f - (2.0f * i + 1.0f) * (1.0f / n);
                float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);

                return vec3(cosf(phi) * sinTheta, sinf(phi) * sinTheta, cosTheta);
            };
            // Ray sorting function which sorts rays based on being oriented similarly
            auto sortRays = [](std::vector<vec4>& rayDirs) {
                // Sort rays into 8 bins
                std::sort(rayDirs.begin(), rayDirs.end(), [=](vec4 ele0, vec4 ele1) -> bool {
                    uint8_t orient0 = 0, orient1 = 0;
                    if (ele0.y > 0.0f) orient0 |= (1 << 0);
                    if (ele0.x > 0.0f) orient0 |= (1 << 1);
                    if (ele0.z > 0.0f) orient0 |= (1 << 2);
                    if (ele1.y > 0.0f) orient1 |= (1 << 0);
                    if (ele1.x > 0.0f) orient1 |= (1 << 1);
                    if (ele1.z > 0.0f) orient1 |= (1 << 2);
                    return orient0 < orient1;
                    });
            };

            std::vector<vec4> rayDirs;
            // Calculate and fill buffer for active probes
            auto rayCount = float(rayDirBuffer.GetElementCount());
            for (float i = 0.0f; i < rayCount; i += 1.0f)
                rayDirs.push_back(vec4(fibonacciSphere(i, rayCount), 0.0));
            //sortRays(rayDirs);
            rayDirBuffer.SetData(rayDirs.data(), 0, rayDirs.size());

            rayDirs.clear();

            // Calculate and fill buffer for inactive probes
            rayCount = float(rayDirInactiveBuffer.GetElementCount());
            for (float i = 0.0f; i < rayCount; i += 1.0f)
                rayDirs.push_back(vec4(fibonacciSphere(i, rayCount), 0.0));
            //sortRays(rayDirs);
            rayDirInactiveBuffer.SetData(rayDirs.data(), 0, rayDirs.size());
        }

        void InternalIrradianceVolume::ClearProbes(ivec2 irrRes, ivec2 momRes, ivec3 probeCount, int32_t cascadeCount) {
            // Fill probe textures with initial values
            std::vector<float> irrVector(irrRes.x * irrRes.y * 4);
            std::vector<float> momVector(momRes.x * momRes.y * 2);

            std::fill(irrVector.begin(), irrVector.end(), 0.0f);
            std::fill(momVector.begin(), momVector.end(), 1000.0f);

            auto [irradianceArray, momentsArray] = GetCurrentProbes();

            for (int32_t i = 0; i < probeCount.y * cascadeCount; i++) {

                irradianceArray0.SetData(irrVector, i);
                momentsArray0.SetData(momVector, i);

                irradianceArray1.SetData(irrVector, i);
                momentsArray1.SetData(momVector, i);

            }

            // Fill probe state buffer with values of 0 (indicates a new probe)
            uint32_t zero = 0;
            std::vector<vec4> probeStates(probeStateBuffer.GetElementCount());
            std::fill(probeStates.begin(), probeStates.end(), vec4(vec3(0.0f), reinterpret_cast<float&>(zero)));
            probeStateBuffer.SetData(probeStates.data(), 0, probeStates.size());

            ResetProbeOffsets();
        }

        void InternalIrradianceVolume::ResetProbeOffsets() {

            std::vector<vec4> probeOffsets(probeOffsetBuffer.GetElementCount());
            std::fill(probeOffsets.begin(), probeOffsets.end(), vec4(0.0f, 0.0f, 0.0f, 1.0f));
            probeOffsetBuffer.SetData(probeOffsets.data(), 0, probeOffsets.size());

        }
    }

}