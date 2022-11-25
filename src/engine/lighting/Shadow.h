#ifndef AE_SHADOW_H
#define AE_SHADOW_H

#include "../System.h"
#include "../Camera.h"
#include "../Framebuffer.h"
#include "../texture/Cubemap.h"
#include "../texture/Texture2DArray.h"

#define MAX_SHADOW_CASCADE_COUNT 5

namespace Atlas {

	namespace Lighting {

		struct ShadowComponent {

			float nearDistance;
			float farDistance;

			mat4 viewMatrix;
			mat4 projectionMatrix;

			mat4 frustumMatrix;
			mat4 terrainFrustumMatrix;

		};

		class Shadow {

		public:
			Shadow(float distance, float bias, int32_t resolution, int32_t numCascades, float splitCorrection);

			Shadow(float distance, float bias, int32_t resolution, bool useCubemap = false);

			void Update();

			float distance = 300.0f;
			float longRangeDistance = 1024.0f;
			float bias = 0.001f;
			float splitCorrection = 0.95f;

			float cascadeBlendDistance = 2.5f;

			int32_t resolution;

			std::vector<ShadowComponent> components;
			int32_t componentCount;

			Texture::Texture2DArray maps;
			Texture::Cubemap cubemap;

			bool useCubemap = false;
			bool allowDynamicActors = false;
			bool allowTerrain = false;
			bool longRange = false;
			bool update = true;

		};

	}

}


#endif