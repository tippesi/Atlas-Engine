#ifndef SHADOW_H
#define SHADOW_H

#include "../System.h"
#include "../Camera.h"
#include "../Framebuffer.h"
#include "../Cubemap.h"

#define MAX_SHADOW_CASCADE_COUNT 4

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

	Texture* maps;
	Cubemap* cubemap;

	RenderList* renderList;

	bool useCubemap;
	bool allowDynamicActors;
	bool update;
	

};


#endif