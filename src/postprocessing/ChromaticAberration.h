#ifndef AE_CHROMATICABBERATION_H
#define AE_CHROMATICABBERATION_H

#include "../System.h"

class ChromaticAberration {

public:
	///
	/// \param strength
	/// \param colorsReversed
	ChromaticAberration(float strength, bool colorsReversed = false) : 
		strength(strength), colorsReversed(colorsReversed) {};

	float strength;
	bool colorsReversed;

};


#endif