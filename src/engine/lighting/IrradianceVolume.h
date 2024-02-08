#pragma once

#include "../System.h"

#include "../volume/AABB.h"
#include "../buffer/Buffer.h"
#include "../texture/Texture2DArray.h"
#include "../terrain/Terrain.h"

namespace Atlas {

    namespace Renderer {

        class DDGIRenderer;
        class MainRenderer;

    }

    namespace Lighting {

        class InternalIrradianceVolume {
        public:
            InternalIrradianceVolume() = default;

            InternalIrradianceVolume(ivec2 irrRes, ivec2 momRes, ivec3 probeCount);

            void SetRayCount(uint32_t rayCount, uint32_t rayCountInactive);

            void SwapTextures();

            std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>
                GetCurrentProbes() const;

            std::tuple<const Texture::Texture2DArray&, const Texture::Texture2DArray&>
                GetLastProbes() const;

            void ClearProbes(ivec2 irrRes, ivec2 momRes, ivec3 probeCount);

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

            IrradianceVolume(Volume::AABB aabb, ivec3 probeCount, bool lowerResMoments = true);

            ivec3 GetIrradianceArrayOffset(ivec3 probeIndex);

            ivec3 GetMomentsArrayOffset(ivec3 probeIndex);

            vec3 GetProbeLocation(ivec3 probeIndex);

            void SetAABB(Volume::AABB aabb);

            void SetRayCount(uint32_t rayCount, uint32_t rayCountInactive);

            void SetProbeCount(ivec3 probeCount);

            void ClearProbes();

            void ResetProbeOffsets();

            Volume::AABB aabb;
            ivec3 probeCount;

            vec3 size;
            vec3 cellSize;

            uint32_t rayCount = 100;
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

            InternalIrradianceVolume internal;

        private:
            friend Renderer::DDGIRenderer;
            friend Renderer::MainRenderer;

            int32_t irrRes = 6;
            int32_t momRes = 14;

        };

    }

}