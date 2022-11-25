#ifndef AE_FRUSTUMCULLING_H
#define AE_FRUSTUMCULLING_H

#include "System.h"
#include "Scene.h"

#include "../lighting/DirectionalLight.h"
#include "../lighting/PointLight.h"

namespace Atlas {

	namespace Scene {

		class FrustumCulling {

		public:
			static void CullActorsFromScene(Scene *scene, Camera *camera);

			static void CullLightsFromScene(Scene *scene, Camera *camera);

			static void CullActorsFromShadow(Lighting::Light *light, Scene *scene, Camera *camera);

		private:
			static void CullActorsFromPointShadow(Lighting::PointLight *light, Scene *scene, Camera *camera);

			static void
			CullActorsFromDirectionalShadow(Lighting::DirectionalLight *light, Scene *scene, Camera *camera);

		};

	}

}

#endif