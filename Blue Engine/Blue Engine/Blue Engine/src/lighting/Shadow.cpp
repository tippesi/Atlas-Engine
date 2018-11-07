#include "Shadow.h"

Shadow::Shadow(float distance, float bias, int32_t numCascades) :
	numCascades(numCascades), distance(distance), bias(bias) {

	cascades = new ShadowCascade[numCascades];

	filtering = true;
	numSamples = 16;
	sampleRange = 2.2f;

}

void Shadow::Update(Camera* camera) {

	for (uint32_t i = 0; i < numCascades; i++) {
		
	}

}