#include "FrustumCulling.h"

void FrustumCulling::CullActorsFromScene(Scene* scene, Camera* camera) {



}

void FrustumCulling::CullLightsFromScene(Scene* scene, Camera* camera) {



}

void FrustumCulling::CullActorsFromShadow(Light* light, Scene* scene, Camera* camera) {

	switch (light->type) {
	case DIRECTIONAL_LIGHT: CullActorsFromDirectionalShadow(light, scene, camera); break;
	case POINT_LIGHT: CullActorsFromPointShadow(light, scene, camera); break;
	}

}

void FrustumCulling::CullActorsFromPointShadow(Light* light, Scene* scene, Camera* camera) {



}

void FrustumCulling::CullActorsFromDirectionalShadow(Light* light, Scene* scene, Camera* camera) {



}