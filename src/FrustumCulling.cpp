#include "FrustumCulling.h"

namespace Atlas {

	void FrustumCulling::CullActorsFromScene(Scene *scene, Camera *camera) {


	}

	void FrustumCulling::CullLightsFromScene(Scene *scene, Camera *camera) {


	}

	void FrustumCulling::CullActorsFromShadow(Lighting::ILight *light, Scene *scene, Camera *camera) {

		switch (light->type) {
			case AE_DIRECTIONAL_LIGHT:
				CullActorsFromDirectionalShadow((Lighting::DirectionalLight*) light, scene, camera);
				break;
			case AE_POINT_LIGHT:
				CullActorsFromPointShadow((Lighting::PointLight*) light, scene, camera);
				break;
		}

	}

	void FrustumCulling::CullActorsFromPointShadow(Lighting::PointLight *light, Scene *scene, Camera *camera) {


	}

	void FrustumCulling::CullActorsFromDirectionalShadow(Lighting::DirectionalLight *light, Scene *scene, Camera *camera) {


	}

}