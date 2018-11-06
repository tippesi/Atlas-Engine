#ifndef CHROMATICABBERATION_H
#define CHROMATICABBARATION_H

#include "../System.h"

class ChromaticAberration {

public:
	ChromaticAberration(float strength, bool colorsReversed = false) : 
		strength(strength), colorsReversed(colorsReversed) {};

	float strength;
	bool colorsReversed;

};


#endif