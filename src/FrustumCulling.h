#ifndef FRUSTUMCULLING_H
#define FRUSTUMCULLING_H

#include "System.h"
#include "Scene.h"

class FrustumCulling {

public:
	static void CullActorsFromScene(Scene* scene, Camera* camera);

	static void CullLightsFromScene(Scene* scene, Camera* camera);

	static void CullActorsFromShadow(Light* light, Scene* scene, Camera* camera);

private:
	static void CullActorsFromPointShadow(Light* light, Scene* scene, Camera* camera);

	static void CullActorsFromDirectionalShadow(Light* light, Scene* scene, Camera* camera);

};


#endif