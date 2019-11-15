#ifndef AE_ENVIRONMENTPROBE_H
#define AE_ENVIRONMENTPROBE_H

#include "../System.h"

#include "../texture/Cubemap.h"
#include "../texture/Texture2D.h"

namespace Atlas {

	namespace Lighting {

		class EnvironmentProbe {

		public:
			EnvironmentProbe() {}

			EnvironmentProbe(const Texture::Cubemap& cubemap);

			EnvironmentProbe(int32_t width, int32_t height, 
				vec3 position = vec3(0.0f));

			void SetPosition(vec3 position);

			vec3 GetPosition();

			void Filter();

			std::vector<mat4> matrices;

			Texture::Cubemap cubemap;
			Texture::Texture2D depth;

		private:
			vec3 position = vec3(0.0f);

		};

	}

}

#endif