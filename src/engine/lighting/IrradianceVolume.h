#pragma once

#include "../System.h"

#include "../volume/AABB.h"
#include "../buffer/Buffer.h"
#include "../texture/Texture2DArray.h"
#include "../terrain/Terrain.h"

#define MAX_IRRADIANCE_VOLUME_CASCADES 8

namespace Atlas {

    namespace Renderer {

        class DDGIRenderer;
        class MainRenderer;

    }

    namespace Lighting {

        class InternalIrradianceVolume {
        public:
            InternalIrradianceVolume() = default;

            InternalIrradianceVolume(ivec2 irrRes, ivec2 momRes, ivec3 probeCount, int32_t cascadeCount);

            void SetRayCount(uint32_t rayCount, uint32_t rayCountInactive);

            void SwapTextures();

            std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>
                GetCurrentProbes() const;

            std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>
                GetLastProbes() const;

            void ClearProbes(ivec2 irrRes, ivec2 momRes, ivec3 probeCount, int32_t cascadeCount);

            void ResetProbeOffsets();

            Buffer::Buffer rayDirBuffer;
            Buffer::Buffer rayDirInactiveBuffer;
            Buffer::Buffer probeOffsetBuffer;
            Buffer::Buffer probeStateBuffer;

        private:
            void FillRayBuffers();

            Texture::Texture2DArray irradianceArray0;
            Texture::Texture2DArray momentsArray0;

            Texture::Texture2DArray irradianceArray1;
            Texture::Texture2DArray momentsArray1;

            int32_t swapIdx = 0;

        };

        class IrradianceVolume {

        public:
            IrradianceVolume() = default;

            IrradianceVolume(Volume::AABB aabb, ivec3 probeCount, int32_t cascadeCount = MAX_IRRADIANCE_VOLUME_CASCADES, bool lowerResMoments = true);

            ivec3 GetIrradianceArrayOffset(ivec3 probeIndex, int32_t cascadeIndex);

            ivec3 GetMomentsArrayOffset(ivec3 probeIndex, int32_t cascadeIndex);

            vec3 GetProbeLocation(ivec3 probeIndex, int32_t cascadeIndex);

            void SetAABB(Volume::AABB aabb);

            void SetRayCount(uint32_t rayCount, uint32_t rayCountInactive);

            void SetProbeCount(ivec3 probeCount);

            void ClearProbes();

            void ResetProbeOffsets();

            Volume::AABB aabb[MAX_IRRADIANCE_VOLUME_CASCADES];

            ivec3 probeCount;
            int32_t cascadeCount;

            vec3 size[MAX_IRRADIANCE_VOLUME_CASCADES];
            vec3 cellSize[MAX_IRRADIANCE_VOLUME_CASCADES];

            uint32_t rayCount = 128;
            uint32_t rayCountInactive = 32;

            float hysteresis = 0.98f;
            float bias = 0.3f;
            float sharpness = 50.0f;

            float gamma = 5.0f;

            float strength = 1.0f;

            bool enable = true;
            bool update = true;
            bool sampleEmissives = false;
            bool debug = false;
            bool optimizeProbes = true;
            bool useShadowMap = false;
            bool lowerResMoments = false;
            bool opacityCheck = false;
            bool scroll = true;

            InternalIrradianceVolume internal;

        private:
            friend Renderer::DDGIRenderer;
            friend Renderer::MainRenderer;

            int32_t irrRes = 6;
            int32_t momRes = 14;

        };

    }

}