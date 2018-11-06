#include "PostProcessing.h"

PostProcessing::PostProcessing() {

	exposure = 1.0f;
	saturation = 1.0f;

	filmicTonemapping = false;

	vignette = new Vignette();
	chromaticAberration = new ChromaticAberration();

}

PostProcessing::~PostProcessing() {

	delete vignette;
	delete chromaticAberration;

}