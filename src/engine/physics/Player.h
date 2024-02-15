#pragma once

#include "Shape.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace Atlas::Physics {

    class PhysicsWorld;

	struct PlayerCreationSettings {
		JPH::CharacterVirtualSettings GetSettings() const;

		float maxSlopeAngle = 0.87f;
		vec3 up = vec3(0.0f, 1.0f, 0.0f);

		float mass = 75.0f;
		float maxStrength = 100.0f;

        float predictiveContactDistance = 0.1f;
        float padding = 0.02f;

        vec3 shapeOffset;

		Ref<Shape> shape;
	};

    class Player {

    public:
        Player(const PlayerCreationSettings& creationSettings, Ref<PhysicsWorld>& physicsWorld);



    };

}