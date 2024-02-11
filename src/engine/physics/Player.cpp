#include "Player.h"
#include "MathConversion.h"

namespace Atlas::Physics {

	JPH::CharacterVirtualSettings PlayerCreationSettings::GetSettings() const {

		JPH::CharacterVirtualSettings settings;

		settings.mMaxSlopeAngle = maxSlopeAngle;
		settings.mUp = VecToJPHVec(up);

		settings.mMass = mass;
		settings.mMaxStrength = maxStrength;

		settings.mShape = shape->ref;
		
		return settings;

	}

}