#ifndef AE_SHADOW_H
#define AE_SHADOW_H

#include "../System.h"
#include "../Camera.h"
#include "../Framebuffer.h"
#include "../texture/Cubemap.h"
#include "../texture/Texture2DArray.h"

#define MAX_SHADOW_CASCADE_COUNT 4

namespace Atlas {

	namespace Lighting {

		// Forward declaration of classes
		class RenderList;

		typedef struct ShadowComponent {

			float nearDistance;
			float farDistance;

			mat4 viewMatrix;
			mat4 projectionMatrix;

		}ShadowComponent;

		class Shadow {

		public:
			Shadow(float distance, float bias, int32_t resolution, int32_t numCascades, float splitCorrection);

			Shadow(float distance, float bias, int32_t resolution, bool useCubemap = false);

			void Update();

			~Shadow();

			float distance;
			float bias;
			float splitCorrection;

			int32_t resolution;

			int32_t sampleCount;
			float sampleRange;

			ShadowComponent* components;
			int32_t componentCount;

			Texture::Texture2DArray* maps;
			Texture::Cubemap* cubemap;

			RenderList* renderList;

			bool useCubemap;
			bool allowDynamicActors;
			bool update;


		};

	}

}


#endif