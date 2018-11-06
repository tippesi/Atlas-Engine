#ifndef SHADOW_H
#define SHADOW_H

#include "../System.h"
#include "../Camera.h"

// Forward declaration of light class
class Light;

typedef struct ShadowCascade {

	float nearDistance;
	float farDistance;

	mat4 viewMatrix;
	mat4 projectionMatrix;

}ShadowCascade;

class Shadow {

public:
	Shadow(float distance, float bias, int32_t numCascades = 1);

	void Update(Camera* camera);

	float distance;
	float bias;

	bool filtering;

	int32_t numSamples;
	float sampleRange;

	ShadowCascade* cascades;

	Light* light;

private:
	int32_t numCascades;

};


#endif