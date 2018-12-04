#ifndef FRUSTUMCULLING_H
#define FRUSTUMCULLING_H

#include "System.h"
#include "Scene.h"

#include "lighting/DirectionalLight.h"
#include "lighting/PointLight.h"

class FrustumCulling {

public:
	static void CullActorsFromScene(Scene* scene, Camera* camera);

	static void CullLightsFromScene(Scene* scene, Camera* camera);

	static void CullActorsFromShadow(ILight* light, Scene* scene, Camera* camera);

private:
	static void CullActorsFromPointShadow(PointLight* light, Scene* scene, Camera* camera);

	static void CullActorsFromDirectionalShadow(DirectionalLight* light, Scene* scene, Camera* camera);

};


#endif