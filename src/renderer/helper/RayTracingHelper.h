#ifndef RAYTRACINGHELPER_H
#define RAYTRACINGHELPER_H

#include "../../System.h"
#include "../../shader/Shader.h"
#include "../../scene/Scene.h"
#include "../../buffer/Buffer.h"
#include "../texture/TextureAtlas.h"
#include <functional>


namespace Atlas {

	namespace Renderer {

		namespace Helper {

			class RayTracingHelper {

				friend Scene::RTData;

			public:
				RayTracingHelper();

				void SetScene(Scene::Scene* scene, int32_t textureDownscale = 1, 
					bool useEmissivesAsLights = false);

				void SetRayBufferSize(size_t rayCount);

				void DispatchAndHit(Shader::Shader* dispatchAndHitShader, glm::ivec3 dimensions, std::function<void(void)> prepare);

				void DispatchRayGen(Shader::Shader* rayGenShader, glm::ivec3 dimensions, std::function<void(void)> prepare);

				void DispatchHitClosest(Shader::Shader* hitShader, std::function<void(void)> prepare);

				void DispatchHitAny(Shader::Shader* hitShader, std::function<void(void)> prepare);

				void DispatchGather(Shader::Shader* gatherShader, std::function<void(void)> prepare);

				void InvalidateRayBuffer();

				Buffer::Buffer* GetRayBuffer();

				void UpdateLights();


			private:
				int32_t workGroupLimit;
				int32_t textureUnitCount;

				Scene::Scene* scene;
				
				std::vector<Scene::RTData::GPULight> lights;
				std::vector<Scene::RTData::GPULight> selectedLights;

				Shader::Shader traceDispatchShader;
				Shader::Shader traceClosestShader;
				Shader::Shader traceAnyShader;

				Buffer::Buffer indirectSSBOBuffer;
				Buffer::Buffer indirectDispatchBuffer;

				Buffer::Buffer counterBuffer0;
				Buffer::Buffer counterBuffer1;

				Buffer::Buffer rayBuffer;
				Buffer::Buffer rayPayloadBuffer;

				
				Buffer::Buffer lightBuffer;

				int32_t dispatchCounter;
				int32_t textureDownscale;
				bool useEmissivesAsLights = false;

			};

		}

	}

}

#endif