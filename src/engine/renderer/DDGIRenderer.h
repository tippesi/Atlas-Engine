#ifndef IRRADIANCEVOLUMERENDERER_H
#define IRRADIANCEVOLUMERENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class DDGIRenderer : public Renderer {

		public:
			DDGIRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) final {};

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

			void TraceAndUpdateProbes(Scene::Scene* scene, Graphics::CommandList* commandList);

			void DebugProbes(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene, 
				std::unordered_map<void*, uint16_t>& materialMap);

			// Used for debugging
			Material probeDebugMaterial;
			Material probeDebugActiveMaterial;
			Material probeDebugInactiveMaterial;
			Material probeDebugOffsetMaterial;

		private:
            struct alignas(16) RayGenUniforms {
                mat4 rotationMatrix;
            };

            struct alignas(16) Cascade {
                float distance;
                float texelSize;
                float aligment0;
                float aligment1;
                mat4 cascadeSpace;
            };

            struct alignas(16) Shadow {
                float distance;
                float bias;

                float cascadeBlendDistance;

                int cascadeCount;
                vec2 resolution;

                Cascade cascades[6];
            };

            struct alignas(16) RayHitUniforms {
                float seed;
                Shadow shadow;
            };

			Buffer::Buffer rayHitBuffer;

			Buffer::Buffer rayGenUniformBuffer;
			Buffer::Buffer rayHitUniformBuffer;

			Buffer::VertexArray vertexArray;
			Buffer::VertexArray sphereArray;

			Helper::RayTracingHelper helper;

            PipelineConfig probeDebugPipelineConfig;

            PipelineConfig rayGenPipelineConfig;
            PipelineConfig rayHitPipelineConfig;

            PipelineConfig probeStatePipelineConfig;
            PipelineConfig probeIrradianceUpdatePipelineConfig;
            PipelineConfig probeMomentsUpdatePipelineConfig;

            PipelineConfig irradianceCopyEdgePipelineConfig;
            PipelineConfig momentsCopyEdgePipelineConfig;

            Ref<Graphics::Sampler> shadowSampler;

		};

	}

}

#endif