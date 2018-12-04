#ifndef SHADOW_H
#define SHADOW_H

#include "../System.h"
#include "../Camera.h"
#include "../Framebuffer.h"
#include "ILight.h"

#define MAX_SHADOW_CASCADE_COUNT 4

// Forward declaration of classes
class ILight;
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

	Shadow(float distance, float bias, int32_t resolution);

	void Update(Camera* camera);

	~Shadow();

	float distance;
	float bias;
	float splitCorrection;
	
	int32_t resolution;

	int32_t sampleCount;
	float sampleRange;

	ShadowComponent* components;
	int32_t componentCount;

	Texture* maps;

	ILight* light;

	RenderList* renderList;

private:
	void UpdateShadowComponent(ShadowComponent* cascade, Camera* camera);

};


#endif