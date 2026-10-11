#pragma once

#include "../System.h"
#include "Renderer.h"

#include "../lighting/Atmosphere.h"

namespace Atlas {

    namespace Renderer {


        class AtmosphereRenderer : public Renderer {

        public:
            AtmosphereRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void Render(Ref<Lighting::EnvironmentProbe> probe, Ref<Scene::Scene> scene,
                Graphics::CommandList* commandList);

            static std::string vertexPath;
            static std::string fragmentPath;

        private:
            inline static const ivec2 transmittanceLutResolution = ivec2(256, 64);
            inline static const ivec2 multipleScatteringLutResolution = ivec2(32, 32);
            inline static const ivec2 skyViewLutResolution = ivec2(256, 128);
            inline static const int32_t groupSize = 8;

            struct alignas(16) AtmosphereParameters {
                vec4 rayleighScatteringCoeff = vec4(0.0f);
                vec4 groundAlbedo = vec4(0.0f);
                float mieScatteringCoeff = 0.0f;
                float rayleighHeightScale = 0.0f;
                float mieHeightScale = 0.0f;
                float surfaceHeightOffset = 0.0f;
                float planetRadius = 0.0f;
                float atmosphereRadius = 0.0f;
            };

            struct alignas(16) RenderUniforms {
                mat4 ivMatrix;
                mat4 ipMatrix;
                vec4 cameraLocation;
                vec4 planetCenter;
                vec4 sunDirection;
                vec4 sunRadiance;
                AtmosphereParameters atmosphere;
            };

            struct alignas(16) MultipleScatteringUniforms {
                vec4 sunRadiance;
                AtmosphereParameters atmosphere;
            };

            struct alignas(16) SkyViewUniforms {
                vec4 cameraLocation;
                vec4 planetCenter;
                vec4 sunDirection;
                vec4 sunRadiance;
                AtmosphereParameters atmosphere;
            };

            void UpdateTransmittanceLut(AtmosphereParameters atmosphereParameters,
                Lighting::Atmosphere* atmosphere, Graphics::CommandList* commandList);

            void UpdateMultipleScatteringLut(const MultipleScatteringUniforms& uniforms,
                Lighting::Atmosphere* atmosphere, Graphics::CommandList* commandList);

            void UpdateSkyViewLut(const SkyViewUniforms& uniforms, Lighting::Atmosphere* atmosphere,
                Graphics::CommandList* commandList);

            static AtmosphereParameters GetAtmosphereParameters(const Lighting::Atmosphere& atmosphere,
                const Lighting::Sky& sky);

            static vec3 GetSunDirection(const LightComponent& light);

            static vec3 GetSunRadiance(const LightComponent& light);

            static ivec2 GetGroupCount(ivec2 resolution);

            static bool HasMissingLuts(const Lighting::Atmosphere& atmosphere, bool includeSkyView);
            
            static void EnsureLutTexture(Texture::Texture2D& texture, ivec2 resolution);

            PipelineConfig defaultPipelineConfig;
            PipelineConfig cubeMapPipelineConfig;
            PipelineConfig transmittancePipelineConfig;
            PipelineConfig multipleScatteringPipelineConfig;
            PipelineConfig skyViewPipelineConfig;

            Buffer::UniformBuffer uniformBuffer;
            Buffer::UniformBuffer transmittanceUniformBuffer;
            Buffer::UniformBuffer multipleScatteringUniformBuffer;
            Buffer::UniformBuffer skyViewUniformBuffer;
            Buffer::UniformBuffer probeMatricesBuffer;

        };

    }

}
